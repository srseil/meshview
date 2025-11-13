# meshview

A simple PBR mesh viewer in C++ 20 and OpenGL 4.6. Implementation based on the book _3D Graphics Programming Cookbook_ by Sergey Kosarevsky and Viktor Latypov.

Per default, the program loads the classic DamagedHelmet glTF2 model, using a 1K version of the Piazza Bologni environment map. Both are found in the `data/` sub-directory. The program should be able to load the first listed mesh of all model file formats supported by assimp, as long as the textures required for the PBR shader are in the relevant slots of the assimp scene object (check out `src/mesh.cpp` for what is expected).

You can use WASD to move the first-person camera around, and the left mouse button to drag the camera view direction. F re-aligns the view with the world's up-vector. F12 creates a screenshot and saves it as a PNG file in the output directory.

The program generates the irradiance map for the specified environment map (see `src/main.cpp`) at startup. Depending on how high the resolution for the latter is, this might take a couple of seconds.
