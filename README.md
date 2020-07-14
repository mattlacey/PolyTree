# PolyTree

At some stage this'll be a simple BSP generator of .OBJ files, probably with some kind of export option that spits the model out in either binary or ascii, using 16.16 fixed point for vertices etc.

That's the plan, anyway.

So far it loads .obj files using code from [Atari Falcon Framework](https://github.com/mattlacey/Falcon-030-Framework) which converts them to 16.16, and then this converts them back to floats for rendering with OpenGL. Dear ImGui is used to provide some basic controls, and I'm pretty much in love with it already.
