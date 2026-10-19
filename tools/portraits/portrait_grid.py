"""Write a portrait with a normalized 0.1 grid, to read region boxes for config/portrait-tints.cfg."""
import sys
from PIL import Image, ImageDraw

def main():
    out = sys.argv[1]
    meshes = sys.argv[2:]
    width, height = 420, 840
    sheet = Image.new('RGB', (width * len(meshes), height))
    for index, mesh in enumerate(meshes):
        image = Image.open('materials/textures/portrait-%s.png' % (mesh if '.mesh' in mesh else mesh + '.mesh')).convert('RGB').resize((width, height))
        draw = ImageDraw.Draw(image)
        for step in range(1, 10):
            x = step * width // 10
            y = step * height // 10
            draw.line([(x, 0), (x, height)], fill=(255, 255, 255), width=1)
            draw.line([(0, y), (width, y)], fill=(255, 255, 255), width=1)
            draw.text((x + 2, 2), str(step / 10), fill=(255, 255, 0))
            draw.text((2, y + 2), str(step / 10), fill=(255, 255, 0))
        draw.text((width - 80, height - 14), mesh, fill=(0, 255, 0))
        sheet.paste(image, (index * width, 0))
    sheet.save(out)

main()
