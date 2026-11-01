
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "netdb.h"

#include "libssh2_config.h"
#include <libssh2.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "funktionen_lvgl.h"
#include "ssh.h"

static const char *TAG = "SSH";

extern char SERVER_IP[16];

#ifndef CONFIG_SSH_PORT
#define CONFIG_SSH_PORT 22
#endif

extern char SSH_USER[100];
extern char SSH_PASSWORD[100];
extern char SSH_HOSTNAME[100];
#define SSH_BUFFER_SIZE 128

// Die Verbindung bleibt zwischen den Funktionen erhalten.
static int ssh_sock = -1;
static LIBSSH2_SESSION *ssh_session = NULL;

// --------------------------------------------------
// Warten, bis der Socket wieder bereit ist
// --------------------------------------------------
static int ssh_waitsocket(void)
{
    struct timeval timeout = {
        .tv_sec = 10,
        .tv_usec = 0
    };

    fd_set readfd;
    fd_set writefd;
    fd_set *readptr = NULL;
    fd_set *writeptr = NULL;

    int dir = libssh2_session_block_directions(ssh_session);

    FD_ZERO(&readfd);
    FD_ZERO(&writefd);

    if (dir & LIBSSH2_SESSION_BLOCK_INBOUND) {
        FD_SET(ssh_sock, &readfd);
        readptr = &readfd;
    }

    if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) {
        FD_SET(ssh_sock, &writefd);
        writeptr = &writefd;
    }

    // Falls libssh2 keine Richtung vorgibt, beide prüfen.
    if (readptr == NULL && writeptr == NULL) {
        FD_SET(ssh_sock, &readfd);
        FD_SET(ssh_sock, &writefd);
        readptr = &readfd;
        writeptr = &writefd;
    }

    return select(ssh_sock + 1, readptr, writeptr, NULL, &timeout);
}

// --------------------------------------------------
// SSH VERBINDUNG ÖFFNEN
// Rückgabe: 0 = Erfolg, -1 = Fehler
// --------------------------------------------------
int ssh_open(void)
{
    if (ssh_session != NULL) {
        ESP_LOGI(TAG, "SSH ist bereits verbunden");
        return 0;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(CONFIG_SSH_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &sin.sin_addr) != 1) {
        struct hostent *hp = gethostbyname(SERVER_IP);

        if (hp == NULL || hp->h_addr_list[0] == NULL) {
            ESP_LOGE(TAG, "Servername konnte nicht aufgeloest werden");
            logkonsole("SSH: Server nicht gefunden");
            return -1;
        }

        memcpy(&sin.sin_addr, hp->h_addr_list[0],
               sizeof(sin.sin_addr));
    }

    if (libssh2_init(0) != 0) {
        ESP_LOGE(TAG, "libssh2_init fehlgeschlagen");
        return -1;
    }

    ssh_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (ssh_sock < 0) {
        ESP_LOGE(TAG, "Socket konnte nicht erstellt werden");
        libssh2_exit();
        return -1;
    }

    if (connect(ssh_sock, (struct sockaddr *)&sin,
                sizeof(sin)) != 0) {
        ESP_LOGE(TAG, "TCP-Verbindung fehlgeschlagen: %d", errno);
        logkonsole("SSH: Verbindung fehlgeschlagen");

        close(ssh_sock);
        ssh_sock = -1;
        libssh2_exit();
        return -1;
    }

    ssh_session = libssh2_session_init();

    if (ssh_session == NULL) {
        ESP_LOGE(TAG, "Session konnte nicht erstellt werden");
        close(ssh_sock);
        ssh_sock = -1;
        libssh2_exit();
        return -1;
    }

    // Nicht blockierender Betrieb für die EAGAIN-Schleifen.
    libssh2_session_set_blocking(ssh_session, 0);

    int rc;

    do {
        rc = libssh2_session_handshake(ssh_session, ssh_sock);

        if (rc == LIBSSH2_ERROR_EAGAIN) {
            if (ssh_waitsocket() <= 0) {
                ESP_LOGE(TAG, "Timeout beim SSH-Handshake");
                ssh_close2();
                return -1;
            }
        }
    } while (rc == LIBSSH2_ERROR_EAGAIN);

    if (rc != 0) {
        ESP_LOGE(TAG, "SSH-Handshake fehlgeschlagen: %d", rc);
        ssh_close2();
        return -1;
    }

    do {
        rc = libssh2_userauth_password(
            ssh_session, SSH_USER, SSH_PASSWORD);

        if (rc == LIBSSH2_ERROR_EAGAIN) {
            if (ssh_waitsocket() <= 0) {
                ESP_LOGE(TAG, "Timeout bei SSH-Anmeldung");
                ssh_close2();
                return -1;
            }
        }
    } while (rc == LIBSSH2_ERROR_EAGAIN);

    if (rc != 0) {
        ESP_LOGE(TAG, "SSH-Anmeldung fehlgeschlagen: %d", rc);
        logkonsole("SSH: Anmeldung fehlgeschlagen");
        ssh_close2();
        return -1;
    }

    ESP_LOGI(TAG, "SSH-Verbindung erfolgreich");
    logkonsole("SSH verbunden");

    return 0;
}

// --------------------------------------------------
// BEFEHL SENDEN UND ANTWORT EMPFANGEN
// Rückgabe: Exitcode des Befehls oder -1 bei Fehler
// --------------------------------------------------
int ssh_send_command(const char *command, int test)
{
    if (ssh_session == NULL || ssh_sock < 0) {
        ESP_LOGE(TAG, "Keine SSH-Verbindung");
        logkonsole("SSH: Nicht verbunden");
        return -1;
    }

    if (command == NULL || command[0] == '\0') {
        return -1;
    }

    LIBSSH2_CHANNEL *channel = NULL;
    int rc;

    // Channel öffnen
    do {
        channel = libssh2_channel_open_session(ssh_session);

        if (channel == NULL &&
            libssh2_session_last_error(
                ssh_session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN) {

            if (ssh_waitsocket() <= 0) {
                ESP_LOGE(TAG, "Timeout beim Oeffnen des Channels");
                return -1;
            }
        }
    } while (channel == NULL &&
             libssh2_session_last_error(
                 ssh_session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN);

    if (channel == NULL) {
        ESP_LOGE(TAG, "Channel konnte nicht geoeffnet werden");
        return -1;
    }

    libssh2_channel_handle_extended_data2(
        channel, LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE);

    // Befehl ausführen
    do {
        rc = libssh2_channel_exec(channel, command);

        if (rc == LIBSSH2_ERROR_EAGAIN) {
            if (ssh_waitsocket() <= 0) {
                ESP_LOGE(TAG, "Timeout beim Senden des Befehls");
                libssh2_channel_free(channel);
                return -1;
            }
        }
    } while (rc == LIBSSH2_ERROR_EAGAIN);

    if (rc != 0) {
        ESP_LOGE(TAG, "Befehl konnte nicht gestartet werden: %d", rc);
        libssh2_channel_free(channel);
        return -1;
    }

    // Antwort empfangen
    char buffer[SSH_BUFFER_SIZE + 1];


while (1) {
    rc = libssh2_channel_read(
        channel, buffer, SSH_BUFFER_SIZE);

    if (rc > 0) {
        buffer[rc] = '\0';
        ESP_LOGI(TAG, "Antwort: %.*s", rc, buffer);
    if (test == 1) {
        ESP_LOGI(TAG, "STARTET TEST");

        buffer[rc] = '\0';

        buffer[strcspn(buffer, "\r\n")] = '\0';

        strcpy(SSH_HOSTNAME, buffer);

        size_t len = strlen(SSH_HOSTNAME);
        snprintf(SSH_HOSTNAME + len, sizeof(SSH_HOSTNAME) - len, ":");

        ESP_LOGI(TAG, "SSH Hostname: %s", SSH_HOSTNAME);
    }else {
            logkonsole(buffer);
        }
        ESP_LOGI(TAG, "Antwort: %.*s", rc, buffer);
        ESP_LOGI(TAG, "Empfangen: %d Bytes", rc);
    }
    else if (rc == LIBSSH2_ERROR_EAGAIN) {
        if (ssh_waitsocket() <= 0) {
            ESP_LOGE(TAG, "Timeout beim Lesen");
            libssh2_channel_free(channel);
            return -1;
        }
    }
    else if (rc == 0) {
        ESP_LOGI(TAG, "Channel beendet, EOF=%d",
                 libssh2_channel_eof(channel));
        break;
    }
    else {
        ESP_LOGE(TAG, "Fehler beim Lesen: %d", rc);
        libssh2_channel_free(channel);
        return -1;
    }
}


    // Exitcode des Befehls sichern
    int exitcode = libssh2_channel_get_exit_status(channel);

    // Nur den Channel schließen. SSH bleibt offen.
    do {
        rc = libssh2_channel_close(channel);

        if (rc == LIBSSH2_ERROR_EAGAIN) {
            if (ssh_waitsocket() <= 0) {
                ESP_LOGE(TAG, "Timeout beim Schliessen des Channels");
                libssh2_channel_free(channel);
                return -1;
            }
        }
    } while (rc == LIBSSH2_ERROR_EAGAIN);

    libssh2_channel_free(channel);

    ESP_LOGI(TAG, "Befehl beendet, Exitcode: %d", exitcode);

    return exitcode;
}

// --------------------------------------------------
// SSH VERBINDUNG SCHLIESSEN
// --------------------------------------------------
void ssh_close2(void)
{
    if (ssh_session != NULL) {
        int rc;

        do {
            rc = libssh2_session_disconnect(
                ssh_session, "Normal Shutdown");

            if (rc == LIBSSH2_ERROR_EAGAIN) {
                if (ssh_waitsocket() <= 0) {
                    ESP_LOGW(TAG, "Timeout beim Disconnect");
                    break;
                }
            }
        } while (rc == LIBSSH2_ERROR_EAGAIN);

        libssh2_session_free(ssh_session);
        ssh_session = NULL;
    }

    if (ssh_sock >= 0) {
        close(ssh_sock);
        ssh_sock = -1;
    }

    libssh2_exit();

    ESP_LOGI(TAG, "SSH-Verbindung geschlossen");
    logkonsole("SSH getrennt");
}
