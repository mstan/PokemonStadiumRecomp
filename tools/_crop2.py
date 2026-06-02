from PIL import Image
import sys
src = sys.argv[1]
im = Image.open(src)
im.crop((70, 250, 340, 440)).resize((270*3, 190*3), Image.NEAREST).save('build/battlenow.png')
im.crop((345, 155, 610, 440)).resize((265*2, 285*2), Image.NEAREST).save('build/stadium_now.png')
print('ok')
