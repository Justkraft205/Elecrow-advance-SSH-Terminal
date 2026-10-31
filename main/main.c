#include "bsp_illuminate.h"
#include "esp_hosted.h"
#include "lvgl.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "nvs.h"
#include <stdlib.h>
#include "funktionen_lvgl.h"
#include "ssh.h"
#define TASK_FINISH_BIT BIT0
EventGroupHandle_t xEventGroup = NULL;

lv_obj_t *active_field = NULL;
LV_FONT_DECLARE(lv_font_unscii_8);
lv_obj_t *label_button[100];
lv_obj_t *label;
lv_obj_t *keyboard_container;

char WIFI_SSID[100] = "";
char WIFI_PASS[100] = "";
char SSH_USER[100] = "";
char SSH_PASSWORD[100] = "";
char SSH_HOSTNAME[100] = "";
char ip[32];
char SERVER_IP[16] = "";
static int s_retry_num = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAXIMUM_RETRY      5

static EventGroupHandle_t s_wifi_event_group;
void ssh_task(void *pvParameters);
void senden(const char *befehl);
void create_keyboard(int check, int shift2);

const char *gbuchstaben[] = {
    "Q", "W", "E", "R", "T", "Z",
    "U", "I", "O", "P", "<-", "A",
    "S", "D", "F", "G", "H", "J",
    "K", "L", ":", "123", "<>", "Y",
    "X", "C", "V", "!_", "_!", "B",
    "N", "M", "->", "^^", "UP"
};

const char *kbuchstaben[] = {
    "q", "w", "e", "r", "t", "z",
    "u", "i", "o", "p", "<-", "a",
    "s", "d", "f", "g", "h", "j",
    "k", "l", ".", "123", "<>", "y",
    "x", "c", "v", "!_", "_!", "b",
    "n", "m", "->", "^^"
};

const char *zahlen[] = {
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", "<-", "+",
    "-", "*", "/", "=", "(", ")",
    "{", "}", "[", "]", "ABC", "$",
    "!", "?", "@", "!_!", ".", "<",
    ">", "#", "->", "^^"
};
static char text[100] = "";
int shift = 0;
int wselect = 0;
static char callback[20] = "";
static const char *TAG = "ESP32";
static EventGroupHandle_t wifi_events;
static bool wifi_task_started = false;

void activ_text(lv_event_t *e){active_field = lv_event_get_user_data(e);
lv_label_set_text(active_field, "");}

/* WLAN-Daten speichern */
static void save_wifi(void){
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) != ESP_OK)
    return;
    nvs_set_str(nvs, "ssid", WIFI_SSID);
    nvs_set_str(nvs, "pass", WIFI_PASS);
    nvs_set_str(nvs, "ssh_user", SSH_USER);
    nvs_set_str(nvs, "ssh_pass", SSH_PASSWORD);
    nvs_commit(nvs);
    nvs_close(nvs);
}

/* WLAN-Daten laden */
static bool load_wifi(int select){
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK){;return false;}
    size_t ssid_len = sizeof(WIFI_SSID);
    size_t pass_len = sizeof(WIFI_PASS);
    esp_err_t err1 = nvs_get_str(nvs, "ssid", WIFI_SSID, &ssid_len);
    esp_err_t err2 = nvs_get_str(nvs, "pass", WIFI_PASS, &pass_len);
    if (select == 1){
        size_t suser_len = sizeof(SSH_USER);
        size_t spass_len = sizeof(SSH_PASSWORD);
        esp_err_t err3 = nvs_get_str(nvs, "ssh_user", SSH_USER, &suser_len);
        esp_err_t err4 = nvs_get_str(nvs, "ssh_pass", SSH_PASSWORD, &spass_len);
        ESP_LOGI(TAG, "SSH USer laden: %s", esp_err_to_name(err3));
        ESP_LOGI(TAG, "SSH Passwort laden: %s", esp_err_to_name(err4));}
    ESP_LOGI(TAG, "SSID laden: %s", esp_err_to_name(err1));
    ESP_LOGI(TAG, "Passwort laden: %s", esp_err_to_name(err2));
    ESP_LOGI(TAG, "wurden geladen");
    nvs_close(nvs);
    return err1 == ESP_OK && err2 == ESP_OK;
}


static void event_handler(void *arg,esp_event_base_t event_base,int32_t event_id,void *event_data){
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group,WIFI_FAIL_BIT);
        }

    } else if (event_base == IP_EVENT &&event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "IP-Adresse: " IPSTR,IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* WLAN starten */
static void wifi_init_sta(void){
    s_wifi_event_group = xEventGroupCreate();
    ESP_LOGI(TAG, "IN WIFI STA");
    ESP_LOGI(TAG, "WLAN-Name: %s", WIFI_SSID);
    ESP_LOGI(TAG, "WLAN-Passwort: %s", WIFI_PASS);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,&event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,&event_handler, NULL));
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            }
        }
    };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID,sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASS,sizeof(wifi_config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WLAN verbunden");
    } else {
        ESP_LOGE(TAG, "WLAN-Verbindung fehlgeschlagen");
        abort();
    }
    ESP_ERROR_CHECK(esp_event_handler_unregister( WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    vEventGroupDelete(s_wifi_event_group);
}

/* Touch für LVGL */
static void touch_read_cb(lv_indev_t *indev,lv_indev_data_t *data){
    uint16_t x, y;
    bool pressed;
    if (touch_read() == ESP_OK) {
        get_coor(&x, &y, &pressed);
        data->point.x = x;
        data->point.y = y;
        if (pressed) {data->state = LV_INDEV_STATE_PRESSED;
        } else { data->state = LV_INDEV_STATE_RELEASED;}
    } else {data->state = LV_INDEV_STATE_RELEASED;}}

/* WLAN-Eingabe über den Button */
static void wificonnection(){
        wselect = wselect + 1;
        const char *inhalt = lv_label_get_text(active_field);
        if (wselect == 1){
        logkonsole("WlanName:");
        strcpy(callback, "wificonnection");
        } else if (wselect == 2) {
            logkonsole(inhalt);
            strncat(WIFI_SSID, inhalt,sizeof(WIFI_SSID) - strlen(WIFI_SSID) - 1);
            logkonsole("Wlan Passwort:");
        } else if (wselect == 3){
            logkonsole(inhalt);
            strncat(WIFI_PASS, inhalt,sizeof(WIFI_PASS) - strlen(WIFI_PASS) - 1);
            wificonnection();
        } else if (wselect == 4){
            logkonsole("Verbinde mit WLAN...");
            wifi_init_sta();
            logkonsole("IP des zu verbindenen Gerätes:");
        }else if (wselect == 5){
            logkonsole(inhalt);
            strcpy(SERVER_IP, inhalt);
            logkonsole("Username:");
            configASSERT(xEventGroup != NULL);
        } else if (wselect == 6){
            strcpy(SSH_USER, inhalt);
        logkonsole("Passwort:");
    }else if (wselect == 7) {
        strcpy(SSH_PASSWORD, inhalt);
        ssh_open();
        if (ssh_open() != 0) {
            wselect = 6;
            return;
        }
        save_wifi();
        ssh_send_command("hostname", 1);

        logkonsole("SSH Gestartet. Ihre Befehle:");
    } else if (wselect >= 8){
        ssh_send_command(inhalt, 0);
    logkonsole(SSH_HOSTNAME);
ESP_LOGI(TAG, "SSH User: %s", SSH_HOSTNAME);}}

static void button_event(lv_event_t *e){
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        const char *buchstabe = lv_event_get_user_data(e);
        if (strcmp(buchstabe, "<-") == 0) {
            if (strlen(text) > 0) {text[strlen(text) - 1] = '\0';}
        }else if (strcmp(buchstabe, "!_") == 0 ||strcmp(buchstabe, "_!") == 0) {
            strcat(text, " ");
        }else if (strcmp(buchstabe, "<>") == 0) {
            if (shift == 1) {shift = 0;} else {shift = 1;}
            create_keyboard(1, shift);
        }else if (strcmp(buchstabe, "123") == 0) {
            create_keyboard(1, 2);
        }else if (strcmp(buchstabe, "->") == 0) {
        if (strcmp(callback, "") != 0) {
        if (strcmp(callback, "wificonnection") == 0) {
            if (strcmp(lv_label_get_text(active_field), "CLOSE") == 0){ssh_close2();}
            if (strcmp(lv_label_get_text(active_field), "LOAD") == 0){
                ESP_LOGI(TAG, "LOAD wurde erkannt");
                if (load_wifi(1)) {
                logkonsole("Gespeicherte WLAN-Daten geladen");
                wifi_init_sta();
                wselect = 7;
                if (ssh_open() != 0) {
                logkonsole("Fehlgeschlagen beim Verbinden mit dem Host!");
                ESP_LOGI(TAG, "SSH User: %s", SSH_USER);
                ESP_LOGI(TAG, "SSH Passwort: %s", SSH_PASSWORD);;
                logkonsole("SSH-User Name:");
                wselect = 5;
                return;
                }
                ssh_send_command("hostname", 1);
            } else {
            wificonnection();
            }

            } else{wificonnection();}
        }
    }
}
        else if (strcmp(buchstabe, "ABC") == 0) {create_keyboard(1, 0);
        }else if (strcmp(buchstabe, "UP") == 0) {
            create_keyboard(1, 0);
        }
        else if (strcmp(buchstabe, "^^") == 0) {
            create_keyboard(0,0);
        }
        else {strcat(text, buchstabe);
        }
        lv_label_set_text(active_field, text);
    }
}

static lv_obj_t *keyboard(const char *button_text,int x, int y, int sx, int sy, int check,lv_event_cb_t callback){
    lv_obj_t *button;
    if (check == 1) {button = lv_button_create(keyboard_container);
    } else { button = lv_button_create(lv_screen_active());}
    lv_obj_remove_flag(keyboard_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(button, lv_color_make(128, 128, 128), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x000000),LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_size(button, sx, sy);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_radius(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_t *label_button = lv_label_create(button);
    lv_label_set_text(label_button, button_text);
    lv_obj_set_style_text_font(label_button, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(label_button, LV_OPA_COVER, 0);
    lv_obj_center(label_button);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED,(void *)button_text);
    return button;
}

void create_keyboard(int check, int shift2){
    if (keyboard_container != NULL) {
        lv_obj_delete(keyboard_container);
        keyboard_container = NULL;}
    if (check == 1) {
        keyboard_container = lv_obj_create(lv_screen_active());
        lv_obj_set_size(keyboard_container, 1024, 280);
        lv_obj_set_pos(keyboard_container, 0, 320);
        lv_obj_set_style_bg_opa(keyboard_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(keyboard_container, 0, 0);
        lv_obj_set_style_pad_all(keyboard_container, 0, 0);
        int xh = 0;
        int yh = 0;
        for (int i = 0; i < 34; i++) {
            if (i == 33) {
                xh = 0;
                yh = 0;}else if (i < 11) {
                xh = i * 93;
                yh = 70;}else if (i < 22) {
                xh = (i - 11) * 93;
                yh = 140;}else {
                xh = (i - 22) * 93;
                yh = 210;}
            keyboard(shift2 == 2 ? zahlen[i] :shift2 == 1 ? kbuchstaben[i] :gbuchstaben[i],xh, yh, 93,70, check, button_event);}
    }else {keyboard(gbuchstaben[34], 0, 550, 50 , 50, check,button_event);}}

void open_ssh_app(){
    inputfields(100, 330, 924, 60, "befehl");
    wselect = 0;
    wificonnection();
}

void initalize(){
    ESP_ERROR_CHECK(esp_hosted_init());
    ESP_ERROR_CHECK(esp_hosted_connect_to_slave());
    esp_err_t err2 = nvs_flash_init();
    if (err2 == ESP_ERR_NVS_NO_FREE_PAGES ||err2 == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err2 = nvs_flash_init();}
    ESP_ERROR_CHECK(err2);
    if (i2c_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed");
        return;}
    if (touch_init() != ESP_OK) {
        ESP_LOGE(TAG, "Touch initialization failed");
        return;}
    esp_ldo_channel_handle_t ldo3 = NULL;
    esp_ldo_channel_config_t ldo3_config = { .chan_id = 3,.voltage_mv = 2500,};
    esp_ldo_acquire_channel(&ldo3_config, &ldo3);
    esp_ldo_channel_handle_t ldo4 = NULL;
    esp_ldo_channel_config_t ldo4_config = {.chan_id = 4,.voltage_mv = 3300,};
    esp_ldo_acquire_channel(&ldo4_config, &ldo4);
    display_init();
    lv_indev_t *touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, touch_read_cb);
}

void app_main(void){
    initalize();
    xEventGroup = xEventGroupCreate();
    configASSERT(xEventGroup != NULL);
    set_lcd_blight(40);
    label = lv_obj_create(lv_screen_active());
    lv_obj_set_size(label, 1024, 320);
    lv_obj_set_pos(label, 0, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    create_keyboard(1, 0);
    open_ssh_app();
    lvgl_port_unlock();
    while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
}
