"""Tests du vrai programme : flux binaire, rejeu vidéo, journal et arrêt par signal.

Les tests C vérifient des fonctions isolées. Ici, subprocess lance les exécutables
comme le ferait un utilisateur, pour détecter aussi les erreurs d'intégration.
"""
import csv
import io
import pathlib
import signal
import subprocess
import tempfile

# Retrouver le dépôt à partir de ce script, sans dépendre du dossier courant.
ROOT = pathlib.Path(__file__).resolve().parents[1]
TRACKER = str(ROOT / 'bin/tracker')
GENERATOR = str(ROOT / 'bin/gen_frames')

# Le journal et la configuration de test restent dans un dossier temporaire.
# À la sortie du bloc, Python le nettoie, même si une assertion échoue.
with tempfile.TemporaryDirectory(prefix='pantilt-integration-') as temp:
    work = pathlib.Path(temp)
    config = work / 'test.conf'
    config.write_text('width=128\nheight=96\nlog_path=trace.csv\n', encoding='utf-8')
    # stdout contient la vidéo brute ; stderr contient les positions de référence.
    # capture_output sépare les deux flux pour éviter de corrompre les images.
    source = subprocess.run([GENERATOR, '128', '96', '60'], capture_output=True, check=True)
    expected = list(csv.reader(io.StringIO(source.stderr.decode())))
    # Alimenter directement stdin avec les octets générés. Le délai maximal
    # fait échouer le test si le programme se bloque au lieu de terminer.
    result = subprocess.run([TRACKER, '-c', str(config), '-r', '30'], input=source.stdout,
                            capture_output=True, cwd=work, timeout=10)
    assert result.returncode == 0, result.stderr
    with (work / 'trace.csv').open() as fp:
        rows = list(csv.DictReader(fp))
    assert len(rows) == 60
    # Comparer chaque centre détecté à la vérité du générateur, avec une petite
    # tolérance due à l'échantillonnage. Les angles doivent rester dans les butées.
    for row, (_, x, y) in zip(rows, expected):
        assert row['found'] == '1'
        assert abs(float(row['x']) - int(x)) < 1.5
        assert abs(float(row['y']) - int(y)) < 1.5
        assert 125 <= float(row['pan']) <= 145
        assert 80 <= float(row['tilt']) <= 100
    assert float(rows[-1]['t_ms']) == 1966.667
    assert float(rows[0]['pan']) > 135
    # Une image incomplète doit échouer ; une fin de fichier vide est normale.
    for payload, status in [(b'123', 1), (b'', 0)]:
        result = subprocess.run([TRACKER, '-c', str(config)], input=payload,
                                capture_output=True, cwd=work, timeout=5)
        assert result.returncode == status, result.stderr
    # Le producteur n'envoie rien : SIGTERM doit quand même arrêter le programme.
    # Attendre son message de démarrage garantit que son gestionnaire est installé.
    process = subprocess.Popen([TRACKER], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, cwd=work)
    assert b'SIMULATION' in process.stderr.readline()
    process.send_signal(signal.SIGTERM)
    _, stderr = process.communicate(timeout=3)
    assert process.returncode == 0, stderr
    # Refuser cadence non finie, cadence nulle, rejeu matériel et argument inattendu.
    for args in [['-r', 'nan'], ['-r', '0'], ['-a', '-r', '30'], ['unexpected']]:
        result = subprocess.run([TRACKER, *args], input=b'', capture_output=True, cwd=work)
        assert result.returncode == 2
    # /dev/full simule un disque plein sous Linux. L'erreur peut apparaître
    # seulement à la fermeture du tampon ; elle doit changer le code de sortie.
    if pathlib.Path('/dev/full').exists():
        config.write_text('log_path=/dev/full\n', encoding='utf-8')
        result = subprocess.run([TRACKER, '-c', str(config)], input=b'', capture_output=True, cwd=work)
        assert result.returncode == 1
print('integration: replay/CSV, limits, EOF, partial frames, SIGTERM, CLI errors: OK')
