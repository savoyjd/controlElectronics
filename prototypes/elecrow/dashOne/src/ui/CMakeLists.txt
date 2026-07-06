SET(SOURCES screens/ui_Screen1.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_pitch75_png.c
    images/ui_img_roll75_png.c
    images/ui_img_speedgage350_png.c
    images/ui_img_tempcvtsmall_png.c
    images/ui_img_tempsmall_png.c
    images/ui_img_vertical_temp_gauge_png.c
    images/ui_img_orange_needle_5x125_png.c
    images/ui_img_blue_needle_5x125_png.c)

add_library(ui ${SOURCES})
