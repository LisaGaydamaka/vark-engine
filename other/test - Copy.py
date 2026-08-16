from PIL import Image, ImageDraw

def generate_tiled_texture(filename="alba_tiled.png", tile_size=16, output_size=64):
    # Create a blank output image
    img = Image.new("RGB", (output_size, output_size))
    draw = ImageDraw.Draw(img)

    # Seamless 4×4 checkerboard (tile repeated)
    block = 4
    light_gray = (200, 200, 200)
    dark_gray = (100, 100, 100)
    arrow_color = (30, 80, 160)

    # Draw every pixel using modulo to repeat the tile pattern
    for y in range(output_size):
        for x in range(output_size):
            # Tile coordinates (0–15)
            tx = x % tile_size
            ty = y % tile_size

            # Checkerboard
            if ((tx // block) + (ty // block)) % 2 == 0:
                img.putpixel((x, y), light_gray)
            else:
                img.putpixel((x, y), dark_gray)

            # Draw arrow only inside the first tile (0–15) – we'll draw it once per tile later
            # Actually we need to draw the arrow on each tile; we can either draw polygons per tile
            # or pre-render a tile and paste it. For simplicity, we'll paste a tile.

    # Better: create tile first, then paste
    tile = Image.new("RGB", (tile_size, tile_size))
    tile_draw = ImageDraw.Draw(tile)

    # Fill tile with checkerboard
    for y in range(tile_size):
        for x in range(tile_size):
            if ((x // block) + (y // block)) % 2 == 0:
                tile.putpixel((x, y), light_gray)
            else:
                tile.putpixel((x, y), dark_gray)

    # Draw arrow on the tile
    tip_left, tip_right, tip_y = 7, 8, 2
    base_left, base_right, base_y = 3, 12, 10
    tile_draw.polygon([
        (tip_left, tip_y),
        (tip_right, tip_y),
        (base_right, base_y),
        (base_left, base_y)
    ], fill=arrow_color)

    stem_left, stem_right, stem_top, stem_bottom = 7, 8, base_y, 14
    tile_draw.rectangle([stem_left, stem_top, stem_right, stem_bottom], fill=arrow_color)

    # Now tile the tile onto the output image
    for i in range(0, output_size, tile_size):
        for j in range(0, output_size, tile_size):
            img.paste(tile, (i, j))

    img.save(filename)
    print(f"Tiled texture saved as '{filename}' (size {output_size}×{output_size})")

if __name__ == "__main__":
    generate_tiled_texture()