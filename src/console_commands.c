#include "include/console.h"
#include "include/console_commandlinetools.h"
#include "include/console_commands.h"
#include "string.h"

#include "include/led.h"

////////////////////////////////////////////////////////////////////////


static const char *const general[] = {
                                      "General Options",

                                      "help",
                                      "about",
                                      "hallo",
                                      NULL
};

static const char *const led_static[] = {
                                      "LED Options Static",

                                      "led_rot_an",
                                      "led_rot_aus",
                                      "led_grn_an",
                                      "led_grn_aus",
                                      "led_an",
                                      "led_aus",
                                      "led_switch",
                                      NULL
};

static const char *const led_dynamic[] = {
                                      "LED Options Dynamic",

                                      "led_blink",
                                      "led_blinksw",
                                      "stop",
                                      NULL
};

////////////////////////////////////////////////////////////////////////

void commands(char *eingabe){

  if (strcmp(eingabe, "clear")==0){
            clear();
        }
        else if (strcmp(eingabe, "help")==0){
                    listCommands();
                }
        else if (strcmp(eingabe, "about")==0){
                    status();
        }

        else if (strcmp(eingabe, "hallo")==0){
                    system();
                    sends("Hallo zurueck.");
                    linebreak(1);
        }

        else if (strcmp(eingabe, "led_rot_an")==0){
                    led_rot_an();
                    system();
                    sends("RED - ON");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_grn_an")==0){
                    led_grn_an();
                    system();
                    sends("GRN - ON");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_rot_aus")==0){
                    led_rot_aus();
                    system();
                    sends("RED - OFF");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_grn_aus")==0){
                    led_grn_aus();
                    system();
                    sends("GRN - OFF");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_an")==0){
                    led_an();
                    system();
                    sends("RED - ON");
                    linebreak(1);
                    system();
                    sends("GRN - ON");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_aus")==0){
                    led_aus();
                    system();
                    sends("RED - OFF");
                    linebreak(1);
                    system();
                    sends("GRN - OFF");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_switch")==0){
                    led_switch();
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_blink")==0){
                    led_blink();
                    system();
                    sends("BLINK - ON");
                    linebreak(1);
                    system();
                    sends("press any button to cancel.");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "led_blinksw")==0){
                    led_blinksw();
                    system();
                    sends("blinkswitch - ON.");
                    linebreak(1);
                    system();
                    sends("press any button to cancel");
                    linebreak(1);
        }
        else if (strcmp(eingabe, "stop")==0){
                           stop();
                           system();
                           sends("stop.");
                           linebreak(1);
               }

        else{
            system();
            sends("command not found: ");
            red();
            sends("\"");
            sends(eingabe);
            sends("\"");
            linebreak(1);
        }
}

void listCommandGroup(const char *const *gruppe){

    int i;

    for (i=0 ; gruppe[i] != NULL ; i++){
        if (i == 0){
//            linebreak(1);
            whitefat();
            sends(gruppe[i]);
            linebreak(1);
            treeBeginn();
            linebreak(1);
        }
        else if (i > 0){
            if (gruppe[i+1] != NULL){
                treeMiddle();
                cyan();
                sends(gruppe[i]);
                linebreak(1);
            }
            else if (gruppe[i+1] == NULL){
                            treeEnd();
                            cyan();
                            sends(gruppe[i]);
                            linebreak(1);
            }
        }
    }
}

void listCommands(void){
    listCommandGroup(general);
    listCommandGroup(led_static);
    listCommandGroup(led_dynamic);
}
