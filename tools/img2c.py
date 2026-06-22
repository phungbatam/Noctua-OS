#!/usr/bin/env python3
"""Convert an image to a C source file with raw 32-bit RGB pixel data.
Usage: python3 img2c.py <input_image> <output_c_file> [width] [height]
If width/height not specified, uses image native size.
Output is scaled to fill the given dimensions.
"""

import sys
import os
from PIL import Image

def main():
    if len(sys.argv) < 3:
        print("Usage: img2c.py <input> <output.c> [width] [height]")
        sys.exit(1)

    infile = sys.argv[1]
    outfile = sys.argv[2]
    req_w = int(sys.argv[3]) if len(sys.argv) > 3 else None
    req_h = int(sys.argv[4]) if len(sys.argv) > 4 else None

    im = Image.open(infile).convert("RGB")
    w, h = im.size

    if req_w and req_h:
        im = im.resize((req_w, req_h), Image.LANCZOS)
        w, h = req_w, req_h

    pixels = list(im.getdata())
    pixel_count = w * h

    varname = os.path.splitext(os.path.basename(outfile))[0]

    with open(outfile, 'w') as f:
        f.write(f"// Auto-generated from {os.path.basename(infile)} by img2c.py\n")
        f.write(f"// Do not edit manually\n\n")
        f.write(f"#define BACKGROUND_WIDTH  {w}\n")
        f.write(f"#define BACKGROUND_HEIGHT {h}\n\n")
        f.write(f"static const unsigned int background_data[{pixel_count}] = {{\n")

        for i, (r, g, b) in enumerate(pixels):
            pixel = (r << 16) | (g << 8) | b
            if i % 8 == 0:
                f.write("    ")
            f.write(f"0x{pixel:06X},")
            if i % 8 == 7:
                f.write("\n")

        if pixel_count % 8 != 0:
            f.write("\n")

        f.write("};\n")

    print(f"Generated {outfile}: {w}x{h}, {pixel_count} pixels")

if __name__ == "__main__":
    main()
