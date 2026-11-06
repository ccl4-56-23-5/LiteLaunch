"""Convert a PNG exported from icons/logo.svg into the Windows icon resource."""
from pathlib import Path
import argparse
from PIL import Image
parser = argparse.ArgumentParser()
parser.add_argument('png', type=Path, help='PNG produced by logo_export.cpp')
args = parser.parse_args()
with Image.open(args.png) as source:
    source.convert('RGBA').save(Path(__file__).with_name('app.ico'), sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
