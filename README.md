# pantilt-tracker

Tourelle 2 axes (kit Yahboom, servos métal 270° / 180°) qui garde une cible lumineuse ou colorée au centre de l'image d'une caméra montée dessus. Projet C, CESI A3 FISA S3E.

```
rpicam-vid (YUV420 sur stdout) ─► frame_read ─► vision_detect ─► control_update ─► servo_write ─► PCA9685 ─► servos
                                                     │                                   
                                                     └────────────► log_row (CSV)
```

La caméra bouge avec la tourelle : il suffit de ramener la cible au centre de l'image. Pas de calibration caméra/monde ni de cinématique inverse.

## Matériel

- Raspberry Pi 5, Camera Module, carte PCA9685 16 voies (I2C, adresse 0x40).
- Servos : pan sur voie 0 (270°), tilt sur voie 1 (180°), impulsions 500–2500 µs à 50 Hz.
- **Alimentation servos séparée : 6 V, ≥ 5 A, sur le bornier V+/GND de la PCA9685.** Jamais par le 5 V du Pi. Masses reliées. Condensateur ~1000 µF sur V+.
- Pi → PCA9685 : 3V3 → VCC, GND → GND, GPIO2 → SDA, GPIO3 → SCL.

Vérifications avant le code :

```bash
sudo raspi-config nonint do_i2c 0     # active l'I2C
i2cdetect -y 1                         # doit afficher 40
rpicam-hello --list-cameras            # doit lister la caméra
rpicam-vid -t 2000 -n --codec yuv420 --width 640 --height 480 -o data/test.yuv
ls -l data/test.yuv                    # taille multiple de 460800 (640*480*1.5)
```

## Compiler et tester (PC ou Pi)

```bash
make            # bin/tracker, bin/gen_frames, bin/test_control
make test       # tests de commande + passage sur une séquence synthétique, sans servos
```

Sur le Pi :

```bash
rpicam-vid -t 0 -n --codec yuv420 --width 640 --height 480 --framerate 30 -o - \
    | ./bin/tracker -c tracker.conf
```

`-n` : simulation, aucun accès I2C. Ctrl-C recentre les servos puis coupe les sorties.

## Répartition

Chacun possède un module et ses tests ; les en-têtes de `include/` sont le contrat commun et ne changent qu'après accord du groupe.

| | Module | Contenu | Test sans matériel |
|---|---|---|---|
| **A** | `src/vision.c` | Détection : point lumineux (plan Y), puis couleur (U/V), centroïde, surface minimale | `data/cercle.yuv` contre `data/cercle_attendu.csv` |
| **B** | `src/servo.c` | Pilote PCA9685 sur `/dev/i2c-1`, conversion angle → impulsion, butées, arrêt sûr | Mode `-n`, puis un servo seul à faible course |
| **C** | `src/control.c` | Écart pixel → angle, gain, zone morte, limite de vitesse, cible perdue | `tests/test_control.c` |
| **D** | `src/config.c`, `src/log.c` | Lecture de `tracker.conf` avec erreurs ligne par ligne, journal CSV, intégration Pi, démo | Fichier de config faux → message clair |

`src/main.c` et `src/frame.c` sont déjà écrits ; D en est responsable.

## Étapes

1. **Avant la séance 1** : alimentation, câblage, `i2cdetect`, caméra, une séquence `data/test.yuv` enregistrée.
2. **Séance 1** : chaque module fonctionne seul sur PC (`make test`). B fait bouger un servo à la main.
3. **Séance 2** : intégration sur le Pi avec lampe de téléphone ou LED, réglage de `kp`, journal CSV pour la soutenance.
4. **Bonus** : cible colorée (bouchon de stylo), lissage du centroïde, mesure de latence, rejeu d'un CSV.

## Conventions Git

Une branche par module (`vision`, `servo`, `control`, `config`), petits commits, fusion dans `main` quand `make test` passe.
