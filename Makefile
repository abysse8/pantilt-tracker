# CONSTRUCTION DU PROJET : « make » compile tous les exécutables.
# Une variable avec ?= peut être remplacée depuis la ligne de commande.
# Les lignes de recette commencent par une TABULATION, exigée par Make.

CC ?= cc
# C11, optimisation, informations de débogage et avertissements du compilateur.
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
# Activer les interfaces Linux/POSIX utilisées et chercher les en-têtes dans include/.
CPPFLAGS += -D_DEFAULT_SOURCE -Iinclude
# -lm ajoute la bibliothèque mathématique : sin, cos, fminf, lround, etc.
LDLIBS = -lm
# Modules communs : chaque exécutable réutilise les mêmes implémentations.
LIB_SRC = src/frame.c src/vision.c src/servo.c src/control.c src/config.c src/log.c
# Substitution Make : src/vision.c devient build/vision.o, et ainsi de suite.
LIB_OBJ = $(LIB_SRC:src/%.c=build/%.o)
# Un changement d'en-tête doit déclencher la recompilation des modules concernés.
HEADERS = $(wildcard include/*.h)

all: bin/tracker bin/gen_frames bin/test_control bin/test_modules bin/test_servo_hw bin/simulate

# $@ = cible à produire ; $^ = toutes les dépendances ; $< = première dépendance.
# La partie « | bin » demande la création du dossier sans relier son horodatage.
# Édition des liens : réunir les fichiers objets dans l'exécutable principal.
bin/tracker: build/main.o $(LIB_OBJ) | bin
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
# Tests de commande pure : positions cibles connues et angles attendus.
bin/test_control: tests/test_control.c $(LIB_OBJ) $(HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIB_OBJ) $(LDLIBS)
# Tests vision, fichiers, configuration, journal et limites des servos.
bin/test_modules: tests/test_modules.c $(LIB_OBJ) $(HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIB_OBJ) $(LDLIBS)
# Tests du protocole matériel : --wrap intercepte les appels système Linux.
# Ils inspectent les octets sans exiger de carte branchée.
bin/test_servo_hw: tests/test_servo_hw.c $(LIB_OBJ) $(HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIB_OBJ) $(LDLIBS) -Wl,--wrap=open,--wrap=ioctl,--wrap=write,--wrap=close,--wrap=nanosleep
# Simulation fermée : les angles calculés influencent l'image suivante.
bin/simulate: tests/simulate.c $(LIB_OBJ) $(HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIB_OBJ) $(LDLIBS)
# Générateur de vidéo de test indépendante des angles de la tourelle.
bin/gen_frames: tests/gen_frames.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)
# Règle générique : compiler séparément chaque source C en fichier objet.
build/%.o: src/%.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
# Créer uniquement les dossiers nécessaires ; -p accepte un dossier déjà présent.
bin build data:
	mkdir -p $@

# Tous les tests doivent réussir ; Make s'arrête au premier code de sortie non nul.
test: all
	./bin/test_control
	./bin/test_modules
	./bin/test_servo_hw
	./bin/simulate > data/simulation.csv
	python3 tests/integration.py

# Produire les données de simulation puis le graphique visible dans le README.
demo: all
	./bin/simulate > data/simulation.csv
	python3 tools/plot_demo.py data/simulation.csv docs/demo.svg

# Supprimer seulement les produits de compilation ; les sources sont conservées.
clean:
	rm -rf bin build
# Ces noms décrivent des actions, pas des fichiers à rechercher sur le disque.
.PHONY: all test demo clean
