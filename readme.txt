
This is a very rough howto guide for using dpmodel to compile .smd files (usually used to make HalfLife 1 or 2 models) into .dpm models for use with DarkPlaces, QFusion, and anything else that can load .dpm (see also dpmviewer for example loading and rendering code).

dpmodel has not been designed with user-friendlyness in mind unfortunately, so bare with me on this...

You need to make these files to compile a model:
one or more .smd files (one of which is the mesh which is typically not animated, the others are animations).
a .txt dpmodel script file describing what .smd files to put into the .dpm model, as well as any transforms needed for the conversion (rotate, scale, and origin commands).
some textures (usually as .tga) which should be placed in the same locations as the .bmp files the smd refers to (for example if it refers to models/blah.bmp, you will want to put the texture somewhere like quake/id1/models/blah.tga if you are making a model like id1/models/blah.dpm).

The script file should look like this: (# is comment)
# save the model as models/blah.dpm
model models/blah
# move the model this much before saving
origin 0 0 0
# rotate the model -90 degrees around vertical
rotate -90
# scale the model by this amount, 0.5 would be half size and 2.0 would be doule size
scale 1
# load the mesh file, this is stored into the dpm as frame 0
scene mesh.smd
# load an animation, this is stored into the dpm as frames 1 onward
scene idle1.smd
# load an animation, this is stored as frames after the ones occupied by idle1.smd
scene fire.smd

To compile it, run dpmodel with a commandline such as:
dpmodel blah.txt

There is an included box.txt and box.smd for example purposes, here is the information print out from "dpmodel box.txt":
READ: box.txt of size 316
executing command #
executing command model
executing command #
executing command origin
executing command #
executing command rotate
executing command #
executing command scale
executing command #
executing command scene
READ: box.smd of size 3207
parsing scene box
parsetriangles: done
model stats:
24 vertices 12 triangles 1 bones 1 shaders 1 frames
renderlist:
   12 tris    24 verts : boxtexture
file size:     2k
wrote file box.dpm

This also wrote box.h, which contains useful #define's for use of frames in the model in C-language game code, feel free to discard the file if it is not useful.

That is all there is to it.


