/* SIMULATION LOGICIELLE EN BOUCLE FERMÉE — pas une validation du matériel.
 * Cible dans le monde → image artificielle selon l'orientation courante →
 * vrais modules vision/commande → nouvelle orientation simulée.
 * Le modèle suppose que les servos atteignent instantanément les consignes,
 * sans inertie, jeu ni délai caméra : le montage réel sera moins idéal. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

int main(void)
{
    /* Une petite image accélère la simulation. Les butées sont élargies pour
     * ce modèle uniquement : on ne reprend pas ces valeurs pour un premier essai réel. */
    Config c; config_defaults(&c);
    c.width=320; c.height=240; c.vision.min_area=10;
    c.control.max_speed_deg_s=45; c.control.deadband_px=4;
    c.pan.min_deg=90; c.pan.max_deg=180;
    c.tilt.min_deg=55; c.tilt.max_deg=125;
    /* La carte est explicitement simulée (dev=NULL), mais servo_write applique
     * encore ses bornes et les mêmes conversions que pour le matériel. */
    Frame f; assert(frame_alloc(&f,c.width,c.height)==0);
    Pca9685 p; assert(pca9685_open(&p,NULL,0x40)==0);
    ControlState s={.pan=135,.tilt=90};
    /* Le CSV contient la vérité du modèle et le résultat du contrôleur :
     * on peut ainsi comparer la direction de la cible et celle de la tourelle. */
    puts("t_s,visible,found,target_pan,pan,target_tilt,tilt,error_px");
    int found=0, lost=0; double error_sum=0;
    /* 360 images à 30 images/s représentent douze secondes. */
    for(int k=0;k<360;k++) {
        double time=k/30.0;
        float world_pan=150, world_tilt=98;
        /* Cible fixe les trois premières secondes, puis mouvement régulier
         * sur les deux axes : c'est l'entrée de notre expérience. */
        if(time>=3) {
            world_pan+=(float)(8*sin(0.5*(time-3)));
            world_tilt+=(float)(6*sin(0.4*(time-3)));
        }
        /* Masquage volontaire entre 5 et 6 secondes pour tester la perte. */
        int visible=!(k>=150 && k<180);
        /* Projection simplifiée : si la caméra tourne vers la cible, son écart
         * angulaire diminue et le point se rapproche du centre de l'image.
         * C'est ce retour du résultat vers l'entrée qui ferme la boucle. */
        int x=(int)lroundf(c.width/2.0f+(world_pan-s.pan)*c.width/c.control.hfov_deg);
        int y=(int)lroundf(c.height/2.0f+(world_tilt-s.tilt)*c.height/c.control.vfov_deg);
        memset(f.y,40,(size_t)c.width*c.height);
        memset(f.u,128,(size_t)c.width*c.height/2);
        f.y[0]=255; /* Petit distracteur séparé : il ne doit pas gagner la détection. */
        /* Dessiner la vraie cible dans le plan Y uniquement si elle est visible. */
        if(visible) for(int dy=-6;dy<=6;dy++) for(int dx=-6;dx<=6;dx++) {
            int px=x+dx, py=y+dy;
            if(px>=0 && px<c.width && py>=0 && py<c.height && dx*dx+dy*dy<=36)
                f.y[(size_t)py*c.width+px]=255;
        }
        /* Exécuter les MÊMES fonctions que main.c, sans imiter leurs résultats.
         * Vérifier ici que le détecteur voit exactement les images attendues. */
        Target target; assert(vision_detect(&f,&c.vision,&target)==0);
        assert(target.found==visible);
        float old_pan=s.pan, old_tilt=s.tilt;
        control_update(&c.control,&target,c.width,c.height,1.0f/30,&s);
        assert(servo_write(&p,&c.pan,s.pan)==0 && servo_write(&p,&c.tilt,s.tilt)==0);
        s.pan=c.pan.angle; s.tilt=c.tilt.angle;
        /* Avec 45°/s et dt=1/30 s, chaque incrément reste inférieur à 1,5°.
         * La petite marge absorbe uniquement l'arrondi des nombres flottants. */
        assert(fabsf(s.pan-old_pan)<=1.501f && fabsf(s.tilt-old_tilt)<=1.501f);
        /* À la fin de la phase fixe, le suivi doit avoir suffisamment convergé. */
        if(k==89) assert(fabsf(world_pan-s.pan)<1.2f && fabsf(world_tilt-s.tilt)<1.2f);
        /* Distance au centre après correction, calculée depuis la vérité du
         * modèle. Elle reste calculable même pendant le masquage volontaire. */
        float error=hypotf((world_pan-s.pan)*c.width/c.control.hfov_deg,
                           (world_tilt-s.tilt)*c.height/c.control.vfov_deg);
        if(!visible) { lost++; assert(s.pan==old_pan && s.tilt==old_tilt); }
        else { found++; if(k>=210) error_sum+=error; }
        printf("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n",
            time,visible,target.found,world_pan,s.pan,world_tilt,s.tilt,error);
    }
    /* Vérifier le bilan de l'expérience et l'erreur moyenne en fin de suivi.
     * Ce seuil est propre au modèle ; il ne prédit pas la précision matérielle. */
    assert(found==330 && lost==30 && error_sum/150<15);
    fprintf(stderr,"simulation: 360 frames, 330 detected, 30 hidden; late mean error %.2f px\n",error_sum/150);
    frame_free(&f); pca9685_close(&p);
    return 0;
}
