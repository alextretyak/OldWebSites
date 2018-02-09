varying vec3 Vertex,Normal;
uniform vec3 Eye,Light;
uniform mat4 TransformMat;

uniform sampler2D refltex;
uniform sampler3D noisetex;

uniform vec4 deltaPhase;

const vec3 diffuse=vec3(.3,.6,.6),
		   specular=vec3(3.,3.,2.5);
const float shininess=200.;

const float distortion=10.;//determines how normal will affect displacement of texture look-up texel position

void main()
{
	vec3 N=normalize(Normal);
	float d=dot(N,Light);
	vec3 EV=Eye-Vertex;

	float noiseK=texture3D(noisetex,Vertex*.04).a*.2
				+texture3D(noisetex,Vertex*.02).a*.1
				+texture3D(noisetex,Vertex*.01).a*.05+.7;

	vec3 rayDirn=2.*dot(N,Vertex)*N-Vertex;
	vec4 pos=TransformMat*vec4(Vertex+rayDirn*((distortion+Vertex.z)/rayDirn.z-1.),1.);

	gl_FragColor=vec4((diffuse *max(0.,d)
					  +specular*max(0.,pow(dot(2.*d*N-Light,EV)*inversesqrt(dot(EV,EV)),shininess)))*noiseK
					  +texture2D(refltex,pos.xy/pos.w*.5+.5).rgb*.25,1.);
}
