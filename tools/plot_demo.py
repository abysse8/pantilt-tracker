"""Transforme le CSV de simulation en graphique SVG, sans bibliothèque externe.

Ce script présente les résultats ; il ne participe ni à la détection ni à la
commande des servos. Le SVG est aussi visible dans le README sur GitHub.
"""
import csv
import html
import pathlib
import sys

# Deux arguments : chemin du CSV d'entrée et chemin du SVG à créer.
# Convertir les cellules en nombres permet les calculs de coordonnées du tracé.
source, destination = map(pathlib.Path, sys.argv[1:])
with source.open() as fp:
    rows = [{key: float(value) for key, value in row.items()} for row in csv.DictReader(fp)]
# Construire un document SVG : formes et textes sont décrits avec du XML.
# viewBox fixe un repère de 960 × 670 unités, redimensionnable par le navigateur.
# title/desc donnent une description textuelle, notamment aux lecteurs d'écran.
parts = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 960 670" role="img" '
         'aria-labelledby="title desc">',
         '<title id="title">Suivi de cible : simulation logicielle en boucle fermee</title>',
         '<desc id="desc">Cible fixe pendant trois secondes, puis mobile. '
         'Entre cinq et six secondes, elle disparait et la tourelle maintient sa position.</desc>',
         '<rect width="960" height="670" fill="#101827"/>',
         '<style>text{font-family:system-ui,sans-serif;fill:#d8e5f5;font-size:13px}'
         '.grid{stroke:#334155;stroke-width:1}</style>',
         '<text x="36" y="32" style="font-size:22px;font-weight:bold">Demo C — suivi sur deux axes</text>',
         '<text x="36" y="56">Simulation logicielle, 30 images/s. Aucun materiel connecte.</text>']


# Ajouter une étiquette. html.escape empêche qu'un texte contenant < ou &
# soit pris pour une balise XML. La couleur est facultative.
def text(x, y, value, color=None):
    fill = f' style="fill:{color}"' if color else ''
    parts.append(f'<text x="{x}" y="{y}"{fill}>{html.escape(str(value))}</text>')


# Dessiner un panneau : grille, axes, intervalle masqué et une ou deux courbes.
# low/high sont les limites verticales ; series associe colonne CSV et couleur.
def panel(top, label, low, high, series):
    left, width, height = 75, 845, 135
    text(36, top - 12, label)
    # Changer de repère : secondes/valeurs physiques → coordonnées du dessin.
    # L'axe vertical SVG descend ; 1 - (...) remet les grandes valeurs en haut.
    def point(t, v):
        return left + width * t / 12, top + height * (1 - (v-low)/(high-low))
    # La bande orange couvre 5..6 s : la cible est cachée pendant cet intervalle.
    shade_x, _ = point(5, low)
    parts.append(f'<rect x="{shade_x}" y="{top}" width="{width/12}" height="{height}" fill="#7c5827" opacity="0.4"/>')
    # Tracer cinq niveaux horizontaux, puis une verticale toutes les deux secondes.
    for i in range(5):
        v = low + (high-low)*i/4
        _, y = point(0, v)
        parts.append(f'<path class="grid" d="M{left},{y}h{width}"/>')
        text(36, y+4, f'{v:g}')
    for tick in range(0,13,2):
        x, _ = point(tick, low)
        parts.append(f'<path class="grid" d="M{x},{top}v{height}"/>')
        text(x-4, top+height+19, tick)
    # Une polyline relie tous les échantillons dans l'ordre chronologique.
    # Le format .1f suffit pour un dessin, sans alourdir le fichier SVG.
    for key, color in series:
        points = ' '.join(f'{x:.1f},{y:.1f}' for x, y in (point(r['t_s'],r[key]) for r in rows))
        parts.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="2.5"/>')

# Trois panneaux partagent le même temps : pan, tilt, puis erreur au centre.
text(75, 85, 'Cible (degres)', '#60a5fa')
text(245, 85, 'Tourelle (degres)', '#4ade80')
text(475, 85, 'Bande orange : cible masquee, position maintenue')
panel(120, 'Pan — gauche / droite (degres)', 130, 160,
      [('target_pan','#60a5fa'), ('pan','#4ade80')])
panel(310, 'Tilt — haut / bas (degres)', 85, 105,
      [('target_tilt','#60a5fa'), ('tilt','#4ade80')])
panel(500, 'Ecart au centre (pixels, apres correction ; modele ideal)', 0, 100,
      [('error_px','#fbbf24')])
text(850, 663, 'Temps (s)')
parts.append('</svg>')
# Créer le dossier si nécessaire, puis enregistrer un fichier UTF-8 autonome.
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text('\n'.join(parts)+'\n', encoding='utf-8')
print(destination)
