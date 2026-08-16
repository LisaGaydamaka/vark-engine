from PIL import Image, ImageDraw

def generate_debug_texture(filename="alba.png", size=16):
    img = Image.new("RGB", (size, size))
    draw = ImageDraw.Draw(img)

    # Seamless 4×4 checkerboard
    block = 4
    light_gray = (200, 200, 200)
    dark_gray = (100, 100, 100)

    for y in range(size):
        for x in range(size):
            if ((x // block) + (y // block)) % 2 == 0:
                img.putpixel((x, y), light_gray)
            else:
                img.putpixel((x, y), dark_gray)

    # Dark contrasting arrow (deep blue)
    arrow_color = (30, 80, 160)

    # Tip: 2 px wide
    tip_left = 7
    tip_right = 8
    tip_y = 2

    # Arrowhead base: 10 px wide (centered)
    base_left = 3
    base_right = 12
    base_y = 10

    draw.polygon([
        (tip_left, tip_y),
        (tip_right, tip_y),
        (base_right, base_y),
        (base_left, base_y)
    ], fill=arrow_color)

    # Stem: 2 px wide, now 1 pixel longer (bottom = 14 instead of 13)
    stem_left = 7
    stem_right = 8
    stem_top = base_y
    stem_bottom = 14   # extended by 1 pixel

    draw.rectangle([stem_left, stem_top, stem_right, stem_bottom], fill=arrow_color)

    img.save(filename)
    print(f"Texture saved as '{filename}' (Alba)")

if __name__ == "__main__":
    generate_debug_texture()