#ifndef FTP_TRANSFER_UI_H
#define FTP_TRANSFER_UI_H
#include "filemanager.h"
#include "userinterface.h"
#if U64 == 1
#include "ftp_client.h"
// Observer is scoped to the task running this copy. Background FTP/HTTP work
// cannot update its UI, and all return paths remove the callback before exit.
class FTPTransferUI {
    UserInterface *ui;
    bool existing, shown, observing;
    static void update(void *context, const char *stage, uint32_t bytes) {
        FTPTransferUI *self = (FTPTransferUI *)context;
        if (!self->ui) return;
        if (!self->existing && !self->shown) { self->ui->show_progress("FTP transfer...", 1); self->shown = true; }
        char text[80]; snprintf(text, sizeof(text), "%s: %u bytes", stage, (unsigned)bytes);
        self->ui->update_progress(text, 0);
    }
public:
    FTPTransferUI(UserInterface *ui, bool existing) : ui(ui), existing(existing), shown(false), observing(false) {
        if (ui) observing = FTPClient::observe_transfers(update, this);
    }
    ~FTPTransferUI() {
        if (observing) FTPClient::observe_transfers(NULL, NULL);
        if (shown) ui->hide_progress();
    }
};
#endif
static FRESULT copy_with_ftp_progress(UserInterface *ui, FileManager *fm, const char *path,
    const char *name, const char *destination, const char *output, bool overwrite, bool existing_progress = false)
{
#if U64 == 1
    FTPTransferUI progress(ui, existing_progress);
#endif
    return fm->fcopy(path, name, destination, output, overwrite);
}
#endif
