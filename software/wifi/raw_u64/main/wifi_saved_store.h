#ifndef WIFI_SAVED_STORE_H
#define WIFI_SAVED_STORE_H
// Included after NVS, ConnectCommand_t and the command-buffer definitions.
static esp_err_t saved_ap_command(ConnectCommand_t *cmd)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("recent_aps", NVS_READWRITE, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND && cmd->command == CMD_WIFI_SAVED_LIST) {
        cmd->buf->data[8] = 0; cmd->buf->size = 9; return ESP_OK;
    }
    if (err != ESP_OK) return err;
    char key[4] = {'s', 'a', '0', 0};
    if (cmd->command == CMD_WIFI_SAVED_LIST) {
        uint8_t count = 0;
        int8_t last = -1;
        nvs_get_i8(h, "last", &last);
        for (int i = 0; i < WIFI_SAVED_MAX; ++i) {
            wifi_saved_record r = {0};
            key[0] = 's'; key[1] = 'a' + i;
            size_t len = sizeof(r.ssid);
            err = nvs_get_str(h, key, r.ssid, &len);
            if (err == ESP_ERR_NVS_NOT_FOUND) continue;
            if (err != ESP_OK) break;
            r.slot = i; r.preferred = (i == last);
            key[0] = 'm';
            err = nvs_get_u8(h, key, &r.auth);
            if (err != ESP_OK) { err = ESP_ERR_INVALID_STATE; break; }
            memcpy(cmd->buf->data + 9 + count * sizeof(r), &r, sizeof(r));
            ++count;
        }
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
        cmd->buf->data[8] = err == ESP_OK ? count : 0;
        cmd->buf->size = 9 + (err == ESP_OK ? count * sizeof(wifi_saved_record) : 0);
    } else {
        char actual[33]; size_t len = sizeof(actual);
        key[1] = 'a' + cmd->list_index;
        err = nvs_get_str(h, key, actual, &len);
        // Reject a stale selection rather than acting on a newly reused slot.
        if (err == ESP_OK && strcmp(actual, cmd->ssid)) err = ESP_ERR_INVALID_STATE;
        if (err == ESP_OK && cmd->command == CMD_WIFI_SAVED_FORGET) {
            const char fields[] = {'s', 'p', 'm'};
            for (unsigned i = 0; i < sizeof(fields); ++i) {
                key[0] = fields[i];
                esp_err_t e = nvs_erase_key(h, key);
                if (e != ESP_OK && e != ESP_ERR_NVS_NOT_FOUND) { err = e; break; }
            }
            int8_t last = -1;
            nvs_get_i8(h, "last", &last);
            if (err == ESP_OK && last == cmd->list_index) {
                esp_err_t e = nvs_erase_key(h, "last");
                if (e != ESP_OK && e != ESP_ERR_NVS_NOT_FOUND) err = e;
            }
            if (err == ESP_OK) err = nvs_commit(h);
        }
    }
    nvs_close(h);
    return err;
}

#endif
