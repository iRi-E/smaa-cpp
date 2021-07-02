# smaa-cpp
An implementation of Enhanced Subpixel Morphological Antialiasing (SMAA) written in C++

## Requirements
- CMake (>= 3.1)
- libpng (optional, needed for tests and example)

## Building
```
mkdir build
cd build
cmake path/to/source
make
sudo make install
```

## API Overview
The folloing two classes are provided. See header files and example (bin/smaa_png.cpp) for more details.

### PixelShader class
Pixel shaders similar to the original SMAA implementation:

smaa-cpp | original HLSL
---------|--------------
SMAA::PixelShader::lumaEdgeDetection()|SMAALumaEdgeDetectionPS()
SMAA::PixelShader::colorEdgeDetection()|SMAAColorEdgeDetectionPS()
SMAA::PixelShader::depthEdgeDetection()|SMAADepthEdgeDetectionPS()
SMAA::PixelShader::blendingWeightCalculation()|SMAABlendingWeightCalculationPS()
SMAA::PixelShader::neighborhoodBlending()|SMAANeighborhoodBlendingPS()

### ImageReader class
This is used for defining getPixel() member function as a callback.

## Platforms
Tested only on Linux.

## License
MIT license.
