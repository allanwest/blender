
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ProjectionMatrix;

uniform float pixsize;   /* rv3d->pixsize */
uniform float pixelsize; /* U.pixelsize */
uniform int keep_size;    
uniform float objscale;

in vec3 pos;
in vec4 color;
in float thickness;

out vec4 finalColor;

#define TRUE 1

float defaultpixsize = pixsize * pixelsize * 40.0;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4( pos, 1.0 );
	finalColor = color;

	if (keep_size == TRUE) {
		gl_PointSize = thickness;
	}
	else {
		float size = (ProjectionMatrix[3][3] == 0.0) ? (thickness / (gl_Position.z * defaultpixsize)) : (thickness / defaultpixsize);
		gl_PointSize = max(size * objscale, 1.0);
	}
	
}
