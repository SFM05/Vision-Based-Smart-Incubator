#include "gui_guider.h"
#include "events_init.h"

lv_ui guider_ui;

void init_scr_del_flag(lv_ui *ui)
{
    ui->screen_1_del = true;
    ui->screen_2_del = true;
}

void setup_ui(lv_ui *ui)
{
    init_scr_del_flag(ui);
    setup_scr_screen_1(ui);
    lv_scr_load(ui->screen_1);
}
