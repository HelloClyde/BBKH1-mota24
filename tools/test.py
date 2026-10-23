"""Build host regressions and render every H1 screen from the actual C code."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TESTS = ("test_core", "test_ui", "test_battle", "test_storage", "test_visual")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--only", nargs="+", choices=TESTS)
    parser.add_argument("--screenshots", type=Path, default=ROOT / "build" / "screenshots")
    args = parser.parse_args()
    output = ROOT / "build" / "tests"
    screenshots = args.screenshots.resolve()
    output.mkdir(parents=True, exist_ok=True)
    screenshots.mkdir(parents=True, exist_ok=True)
    for name in args.only or TESTS:
        exe = output / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run([
            args.cc, "-std=c99", "-O2", "-fno-builtin", "-DMOTA_HOST_TEST",
            "-Wall", "-Wextra", "-Wno-unused-function", "-Wno-unused-variable",
            "-Wno-unused-parameter", "-Wno-misleading-indentation",
            str(ROOT / "tests" / (name + ".c")), "-o", str(exe),
        ], check=True, cwd=ROOT)
        command = [str(exe)]
        if name == "test_visual":
            command.append(str(screenshots))
        subprocess.run(command, check=True, cwd=ROOT)
    if "test_visual" in (args.only or TESTS):
        try:
            from PIL import Image, ImageDraw
        except ImportError:
            print("Pillow unavailable; RGB PPM screenshots remain available.")
        else:
            images = []
            for ppm in sorted(screenshots.glob("*.ppm")):
                with Image.open(ppm) as image:
                    image.save(ppm.with_suffix(".png"))
                    images.append((ppm.stem, image.copy()))
            if images:
                columns, cell_width, cell_height = 3, 496, 304
                rows = (len(images) + columns - 1) // columns
                sheet = Image.new("RGB", (columns * cell_width, rows * cell_height), "#101820")
                draw = ImageDraw.Draw(sheet)
                for index, (name, image) in enumerate(images):
                    x, y = (index % columns) * cell_width, (index // columns) * cell_height
                    draw.text((x + 8, y + 6), name, fill="white")
                    sheet.paste(image, (x + 8, y + 26))
                sheet.save(screenshots / "contact-sheet.png")
            print(f"H1 480x272 screenshots: {screenshots}")
    print("PASS selected H1 Mota regressions")


if __name__ == "__main__":
    main()
