/* TESTS DU MODULE C — aucune caméra et aucun moteur nécessaires.
 * On injecte des positions connues et on vérifie les angles calculés.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "control.h"

int main(void)
{
    /* Gain, champs de vision et sens connus pour prédire les corrections.
     * assert interrompt le test dès qu'une propriété attendue n'est pas vraie. */
    ControlParams p = {.kp=2, .hfov_deg=64, .vfov_deg=48,
        .deadband_px=8, .max_speed_deg_s=30, .pan_dir=1, .tilt_dir=-1};
    ControlState s = {.pan=135, .tilt=90};
    Target t = {.found=true, .x=320, .y=240};
    /* Une cible au centre ne doit déplacer aucun axe. */
    control_update(&p, &t, 640, 480, 0.1f, &s);
    assert(s.pan==135 && s.tilt==90);
    /* Un petit déplacement reste à l'intérieur de la zone morte. */
    t.x=325; t.y=235;
    control_update(&p, &t, 640, 480, 0.1f, &s);
    assert(s.pan==135 && s.tilt==90);
    /* Une grande erreur sature à 30°/s : en 0,1 s, au plus 3°.
     * tilt_dir=-1 doit faire évoluer l'autre axe dans le sens opposé. */
    t.x=620; t.y=440;
    control_update(&p, &t, 640, 480, 0.1f, &s);
    assert(fabsf(s.pan-138)<0.001f && fabsf(s.tilt-87)<0.001f);
    /* Vérifier aussi l'inversion de pan et le calcul hors saturation. */
    p.pan_dir=-1; t.x=420; t.y=240;
    control_update(&p, &t, 640, 480, 0.1f, &s);
    assert(fabsf(s.pan-136)<0.001f);
    float before=s.pan;
    /* Cent images sans cible : aucune consigne ne doit dériver. */
    t.found=false;
    for(int k=0;k<100;k++) control_update(&p,&t,640,480,0.1f,&s);
    assert(s.pan==before && s.lost_count==100);
    /* Retrouver la cible remet le compteur de perte à zéro. */
    t.found=true; t.x=320;
    control_update(&p,&t,640,480,0.1f,&s);
    assert(s.lost_count==0);
    /* Les entrées inutilisables doivent laisser l'état inchangé. */
    control_update(&p,&t,0,480,0.1f,&s);
    control_update(&p,&t,640,480,NAN,&s);
    assert(s.pan==before);
    /* Même durée totale avec deux cadences différentes, à erreur constante :
     * l'angle final doit être identique grâce à l'utilisation de dt. */
    t.x=370; p.pan_dir=1;
    ControlState a={0}, b={0};
    for(int k=0;k<30;k++) control_update(&p,&t,640,480,1.0f/30,&a);
    for(int k=0;k<60;k++) control_update(&p,&t,640,480,1.0f/60,&b);
    assert(fabsf(a.pan-b.pan)<0.001f);
    puts("control: OK");
}
