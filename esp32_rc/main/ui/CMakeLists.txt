SET(SOURCES screens/ui_mainScr.c
    screens/ui_DataScreen.c
    screens/ui_wifiINFOScreen.c
    screens/ui_Screen1.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_1940168468.c
    images/ui_img_sztu1_png.c)

add_library(ui ${SOURCES})
