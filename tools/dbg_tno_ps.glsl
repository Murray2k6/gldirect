#version 150

void clip( float v ) { if ( v < 0.0 ) { discard; } }
void clip( vec2 v ) { if ( any( lessThan( v, vec2( 0.0 ) ) ) ) { discard; } }
void clip( vec3 v ) { if ( any( lessThan( v, vec3( 0.0 ) ) ) ) { discard; } }
void clip( vec4 v ) { if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { discard; } }

float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }
vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }
vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }
vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }

vec4 tex2D( sampler2D sampler, vec2 texcoord ) { return texture( sampler, texcoord.xy ); }
vec4 tex2D( sampler2DShadow sampler, vec3 texcoord ) { return vec4( texture( sampler, texcoord.xyz ) ); }

vec4 tex2D( sampler2D sampler, vec2 texcoord, vec2 dx, vec2 dy ) { return textureGrad( sampler, texcoord.xy, dx, dy ); }
vec4 tex2D( sampler2DShadow sampler, vec3 texcoord, vec2 dx, vec2 dy ) { return vec4( textureGrad( sampler, texcoord.xyz, dx, dy ) ); }

vec4 texCUBE( samplerCube sampler, vec3 texcoord ) { return texture( sampler, texcoord.xyz ); }
vec4 texCUBE( samplerCubeShadow sampler, vec4 texcoord ) { return vec4( texture( sampler, texcoord.xyzw ) ); }

vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }

vec4 tex1Dbias( sampler1D sampler, vec4 texcoord ) { return texture( sampler, texcoord.x, texcoord.w ); }
vec4 tex2Dbias( sampler2D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xy, texcoord.w ); }
vec4 tex3Dbias( sampler3D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }
vec4 texCUBEbias( samplerCube sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }

vec4 tex1Dlod( sampler1D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.x, texcoord.w ); }
vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }
vec4 tex3Dlod( sampler3D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }
vec4 texCUBElod( samplerCube sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }

float dot2 ( vec2 a, vec2 b ) { return dot( a, b ); }
float dot3 ( vec3 a, vec3 b ) { return dot( a, b ); }
float dot3 ( vec3 a, vec4 b ) { return dot( a, b.xyz ); }
float dot3 ( vec4 a, vec3 b ) { return dot( a.xyz, b ); }
float dot3 ( vec4 a, vec4 b ) { return dot( a.xyz, b.xyz ); }
float dot4 ( vec4 a, vec4 b ) { return dot( a, b ); }
float dot4 ( vec2 a, vec4 b ) { return dot( vec4( a, 0.0, 1.0 ), b ); }
vec3 pow3 ( vec3 a, float b ) { return vec3( pow( a.x, b ), pow( a.y, b ), pow( a.z, b ) ); }
vec4 h4texPhys ( sampler2D image, vec2 texcoord ) { return tex2D( image, texcoord ); }
vec4 h4texPhys ( sampler2D image, vec2 texcoord, vec2 dx, vec2 dy ) { return tex2D( image, texcoord, dx, dy ); }
vec2 screenPosToTexcoord ( vec2 pos, vec4 bias_scale ) { return ( pos * bias_scale.zw + bias_scale.xy ); }
const vec4 matrixCoCg1YtoRGB1X = vec4( 1.0, -1.0, 0.0, 1.0 );
const vec4 matrixCoCg1YtoRGB1Y = vec4( 0.0, 1.0, -0.50196078, 1.0 );
const vec4 matrixCoCg1YtoRGB1Z = vec4( -1.0, -1.0, 1.00392156, 1.0 );
uniform vec4 _fa_ [40];
uniform sampler2D samp_dither256map;
uniform sampler2D samp_minlodmap;
uniform sampler2D samp_pagetablemap;
uniform sampler2D samp_physicalmappingsmap;
uniform sampler2D samp_physicalpagesmap0;
uniform sampler2D samp_physicalpagesmap1;
uniform sampler2D samp_physicalpagesmap2;
uniform sampler2D samp_selfshadowmap;
uniform samplerCube samp_dynamicenvmap;

in vec4 gl_FragCoord;
in vec4 vofi_TexCoord0;
in vec4 vofi_TexCoord1;
in vec4 vofi_TexCoord4;
in vec4 vofi_TexCoord2;
in vec4 vofi_TexCoord3;

out vec4 out_FragColor0;
out vec4 out_FragColor1;
out vec4 out_FragColor2;

void main() {
	vec4 sampleSpecular;
	vec4 sampleYCoCg;
	vec4 sampleNormal;
	vec4 feedback;
	vec3 fromViewerN;
	vec3 globalNormal;
	vec4 globalReflection;
	vec3 surfaceDiffuse;
	vec3 surfaceSpecular;
	vec4 color;
	{ if ( _fa_[0 ].x < 1.0 ) {
			vec2 texcoord = gl_FragCoord.xy * ( 1.0 / 256.0 );
			float dither = _fa_[0 ].y * tex2D( samp_dither256map, texcoord ).x + _fa_[0 ].z;
			clip( _fa_[0 ].x - 0.2 - dither * 0.6 );
		}
	};
	{
		vec4 texCoords = vofi_TexCoord0; if ( _fa_[1 ].x > 0 ) {
			vec2 offset = ( texCoords.zw - fract( texCoords.xy ) ) * _fa_[2 ].xy;
			texCoords.xy = fract( texCoords.xy ) * _fa_[2 ].xy + _fa_[2 ].zw;
			texCoords.zw = texCoords.xy + offset;
		}
		float anisoLOD;
		float sampleLOD;
		{
			float widthInTexels = _fa_[3 ].z;
			float maxAnisoLog2 = _fa_[4 ].z;
			vec2 texelCoords = texCoords.xy * widthInTexels;
			vec2 dx = dFdx( texelCoords );
			vec2 dy = dFdy( texelCoords );
			float px = dot2( dx, dx );
			float py = dot2( dy, dy );
			float maxLod = 0.0;
			float minLod = 0.0;
			if ( px > 0.0 && py > 0.0 ) {
				maxLod = 0.5 * log2( max( px, py ) );
				minLod = 0.5 * log2( min( px, py ) );
			}
			anisoLOD = maxLod - min( maxAnisoLog2, maxLod - minLod );
		};
		{
			sampleLOD = anisoLOD;
			if ( _fa_[5 ].x != 0.0 ) {
				float minLod = tex2D( samp_minlodmap, texCoords.xy ).x * 16 - ( 0.49 + _fa_[6 ].x );
				sampleLOD = max( anisoLOD, minLod );
			}
		};
		{
			vec3 physCoords;
			{
				vec4 virtCoordsLod = vec4( texCoords.xy.x, texCoords.xy.y, 0, sampleLOD - 0.5 );
				vec2 physPage = tex2Dlod( samp_pagetablemap, virtCoordsLod ).xy;
				vec4 xform = tex2D( samp_physicalmappingsmap, physPage );
				physCoords.xy = texCoords.xy * xform.x + xform.zw;
				physCoords.z = xform.y;
			};
			{
				sampleSpecular = h4texPhys( samp_physicalpagesmap0, physCoords.xy );
				sampleYCoCg = h4texPhys( samp_physicalpagesmap1, physCoords.xy );
				sampleNormal = h4texPhys( samp_physicalpagesmap2, physCoords.xy );
			};
			if ( _fa_[5 ].y != 0.0 ) {
				vec4 sampleSpecular2, sampleYCoCg2, sampleNormal2;
				{
					vec4 virtCoordsLod = vec4( texCoords.xy.x, texCoords.xy.y, 0, sampleLOD + 0.5 );
					vec2 physPage = tex2Dlod( samp_pagetablemap, virtCoordsLod ).xy;
					vec4 xform = tex2D( samp_physicalmappingsmap, physPage );
					physCoords.xy = texCoords.xy * xform.x + xform.zw;
					physCoords.z = xform.y;
				};
				{
					sampleSpecular2 = h4texPhys( samp_physicalpagesmap0, physCoords.xy );
					sampleYCoCg2 = h4texPhys( samp_physicalpagesmap1, physCoords.xy );
					sampleNormal2 = h4texPhys( samp_physicalpagesmap2, physCoords.xy );
				};
				float trilinearFraction = fract( sampleLOD + _fa_[5 ].z );
				sampleSpecular = mix( sampleSpecular, sampleSpecular2, trilinearFraction );
				sampleYCoCg = mix( sampleYCoCg, sampleYCoCg2, trilinearFraction );
				sampleNormal = mix( sampleNormal, sampleNormal2, trilinearFraction );
			}
		};
		vec2 feedbackTexCoord = texCoords.xy; if ( _fa_[7 ].x > 0.0 ) {
			vec4 sampleSpecular2, sampleYCoCg2, sampleNormal2;
			{
				vec3 physCoords;
				{
					vec4 virtCoordsLod = vec4( texCoords.zw.x, texCoords.zw.y, 0, sampleLOD - 0.5 );
					vec2 physPage = tex2Dlod( samp_pagetablemap, virtCoordsLod ).xy;
					vec4 xform = tex2D( samp_physicalmappingsmap, physPage );
					physCoords.xy = texCoords.zw * xform.x + xform.zw;
					physCoords.z = xform.y;
				};
				{
					sampleSpecular2 = h4texPhys( samp_physicalpagesmap0, physCoords.xy );
					sampleYCoCg2 = h4texPhys( samp_physicalpagesmap1, physCoords.xy );
					sampleNormal2 = h4texPhys( samp_physicalpagesmap2, physCoords.xy );
				};
				if ( _fa_[5 ].y != 0.0 ) {
					vec4 sampleSpecular2, sampleYCoCg2, sampleNormal2;
					{
						vec4 virtCoordsLod = vec4( texCoords.zw.x, texCoords.zw.y, 0, sampleLOD + 0.5 );
						vec2 physPage = tex2Dlod( samp_pagetablemap, virtCoordsLod ).xy;
						vec4 xform = tex2D( samp_physicalmappingsmap, physPage );
						physCoords.xy = texCoords.zw * xform.x + xform.zw;
						physCoords.z = xform.y;
					};
					{
						sampleSpecular2 = h4texPhys( samp_physicalpagesmap0, physCoords.xy );
						sampleYCoCg2 = h4texPhys( samp_physicalpagesmap1, physCoords.xy );
						sampleNormal2 = h4texPhys( samp_physicalpagesmap2, physCoords.xy );
					};
					float trilinearFraction = fract( sampleLOD + _fa_[5 ].z );
					sampleSpecular2 = mix( sampleSpecular2, sampleSpecular2, trilinearFraction );
					sampleYCoCg2 = mix( sampleYCoCg2, sampleYCoCg2, trilinearFraction );
					sampleNormal2 = mix( sampleNormal2, sampleNormal2, trilinearFraction );
				}
			};
			sampleSpecular = mix( sampleSpecular, sampleSpecular2, vofi_TexCoord1.w );
			sampleYCoCg = mix( sampleYCoCg, sampleYCoCg2, vofi_TexCoord1.w );
			sampleNormal = mix( sampleNormal, sampleNormal2, vofi_TexCoord1.w );
			vec2 fp = gl_FragCoord.xy * _fa_[8 ].zw;
			vec2 xy = fract( fp ) - 0.5;
			feedbackTexCoord = ( xy.x * xy.y < 0.0 ) ? texCoords.xy : texCoords.zw;
		}
		{
			float pageSource = _fa_[3 ].x;
			float widthInPages = _fa_[3 ].y;
			float feedbackBias = _fa_[4 ].y;
			feedback.xy = feedbackTexCoord * widthInPages;
			feedback.z = max( 0.0, anisoLOD ) + feedbackBias;
			feedback.w = pageSource;
		};
		{
			feedback = floor( feedback ) / 256.0;
			vec2 xy_low = fract( feedback.xy + 0.5 / 256.0 );
			vec2 xy_high = floor( feedback.xy ) / 256.0;
			vec4 pack;
			pack.xy = xy_low;
			pack.z = xy_high.y * 16.0 + 0.5 / 256 + xy_high.x;
			pack.w = feedback.w * 16.0 + 0.5 / 256 + feedback.z;
			feedback = pack;
		};
	};
	{
		fromViewerN = normalize( vofi_TexCoord4.xyz.xyz );
	};
	{
		vec3 localNormal;
		{
			localNormal = sampleNormal.wyz - 0.5;
			localNormal.z = sqrt( abs( dot( localNormal.xy, localNormal.xy ) - 0.25 ) );
		};
		{
			globalNormal.x = dot3( localNormal, vofi_TexCoord1.xyz );
			globalNormal.y = dot3( localNormal, vofi_TexCoord2.xyz );
			globalNormal.z = dot3( localNormal, vofi_TexCoord3.xyz );
			globalNormal = normalize( globalNormal );
		};
	};
	{
		globalReflection.xyz = fromViewerN - ( globalNormal * dot3( fromViewerN, globalNormal ) * 2.0 );
	};
	{
		sampleYCoCg.z = ( sampleYCoCg.z * 31.875 ) + 1.0;
		sampleYCoCg.z = 1.0 / sampleYCoCg.z;
		sampleYCoCg.xy *= sampleYCoCg.z;
		surfaceDiffuse.x = dot4( sampleYCoCg, matrixCoCg1YtoRGB1X );
		surfaceDiffuse.y = dot4( sampleYCoCg, matrixCoCg1YtoRGB1Y );
		surfaceDiffuse.z = dot4( sampleYCoCg, matrixCoCg1YtoRGB1Z );
	};
	{
		surfaceSpecular = sampleSpecular.xyz * 8.0 / ( sampleNormal.z * 255.0 + 8.0 );
	};
	{
		float graze;
		{
			graze = dot3( -fromViewerN, globalNormal );
			graze = ( _fa_[9 ].x - max( graze, -graze ) ) / max( _fa_[9 ].x, 0.0001 );
		};
		{
			surfaceDiffuse.xyz = mix( surfaceDiffuse.xyz, _fa_[10 ].xyz, saturate( graze * _fa_[10 ].w ) );
		};
		{
			surfaceSpecular.xyz = mix( surfaceSpecular.xyz, _fa_[11 ].xyz, saturate( graze * _fa_[11 ].w ) );
		};
	};
	surfaceDiffuse.xyz = pow3( abs( surfaceDiffuse.xyz ), 2.2 ); if ( _fa_[12 ].x > 0 ) {
		vec3 fragmentXYZ;
		{
			fragmentXYZ = vofi_TexCoord4.xyz + _fa_[13 ].xyz;
		};
		{
			{
				globalReflection.w = ( 1.0 - sampleNormal.x ) * 4.0 + _fa_[14 ].x;
			};
			float powerExp = 4 + sampleNormal.x * 100;
			vec3 selfShadow;
			vec3 thickness;
			float ao;
			{ if( _fa_[12 ].y > 0 ) {
					vec2 texcoord = screenPosToTexcoord( gl_FragCoord.xy, _fa_[15 ] );
					vec4 encoded = tex2D( samp_selfshadowmap, texcoord );
					encoded.xyz *= 2.0;
					selfShadow = max( encoded.xyz - 1.0, vec3( 0.0 ) );
					thickness = encoded.xyz + vec3( _fa_[16 ].y );
					ao = 1.0 - _fa_[17 ].x * ( 1.0 - encoded.w );
				} else {
					selfShadow = vec3( 1.0 );
					thickness = vec3( 100 );
					ao = 1.0;
				}
			};
			vec3 specularLight = vec3( 0.0 );
			vec3 diffuseLight = vec3( 0.0 );
			vec4 light0Color;
			vec3 light0Dir;
			{
				vec3 wsPixelToLight = _fa_[18 ].xyz - fragmentXYZ.xyz;
				float distanceToLight = length( wsPixelToLight );
				float attenuation = 1.0 - ( min( distanceToLight, _fa_[19 ].w ) / max( _fa_[19 ].w, 0.00001 ) );
				light0Dir.xyz = normalize( wsPixelToLight );
				light0Color.xyz = _fa_[19 ].xyz * attenuation * saturate( selfShadow.x + _fa_[18 ].w );
				light0Color.w = attenuation;
			};
			{
				float lightFactor = saturate( dot3( light0Dir, globalNormal ) );
				float lightSpecDot = saturate( dot3( light0Dir, globalReflection ) );
				float lightPow = pow( lightSpecDot, powerExp ) * lightFactor;
				diffuseLight += light0Color.xyz * lightFactor;
				specularLight += light0Color.xyz * lightPow;
			};
			vec4 light1Color;
			vec3 light1Dir;
			{
				vec3 wsPixelToLight = _fa_[20 ].xyz - fragmentXYZ.xyz;
				float distanceToLight = length( wsPixelToLight );
				float attenuation = 1.0 - ( min( distanceToLight, _fa_[21 ].w ) / max( _fa_[21 ].w, 0.00001 ) );
				light1Dir.xyz = normalize( wsPixelToLight );
				light1Color.xyz = _fa_[21 ].xyz * attenuation * saturate( selfShadow.y + _fa_[20 ].w );
				light1Color.w = attenuation;
			};
			{
				float lightFactor = saturate( dot3( light1Dir, globalNormal ) );
				float lightSpecDot = saturate( dot3( light1Dir, globalReflection ) );
				float lightPow = pow( lightSpecDot, powerExp ) * lightFactor;
				diffuseLight += light1Color.xyz * lightFactor;
				specularLight += light1Color.xyz * lightPow;
			};
			vec4 light2Color;
			vec3 light2Dir;
			{
				vec3 wsPixelToLight = _fa_[22 ].xyz - fragmentXYZ.xyz;
				float distanceToLight = length( wsPixelToLight );
				float attenuation = 1.0 - ( min( distanceToLight, _fa_[23 ].w ) / max( _fa_[23 ].w, 0.00001 ) );
				light2Dir.xyz = normalize( wsPixelToLight );
				light2Color.xyz = _fa_[23 ].xyz * attenuation * saturate( selfShadow.z + _fa_[22 ].w );
				light2Color.w = attenuation;
			};
			{
				float lightFactor = saturate( dot3( light2Dir, globalNormal ) );
				float lightSpecDot = saturate( dot3( light2Dir, globalReflection ) );
				float lightPow = pow( lightSpecDot, powerExp ) * lightFactor;
				diffuseLight += light2Color.xyz * lightFactor;
				specularLight += light2Color.xyz * lightPow;
			};
			vec3 ambientLight = vec3( 0.0 )
			+ max( globalNormal.x * _fa_[24 ].xyz, 0 )
			+ max( -globalNormal.x * _fa_[25 ].xyz, 0 )
			+ max( globalNormal.y * _fa_[26 ].xyz, 0 )
			+ max( -globalNormal.y * _fa_[27 ].xyz, 0 )
			+ max( globalNormal.z * _fa_[28 ].xyz, 0 )
			+ max( -globalNormal.z * _fa_[29 ].xyz, 0 );
			diffuseLight += ambientLight;
			vec3 environment;
			{
				environment = texCUBElod( samp_dynamicenvmap, globalReflection ).xyz;;
				environment = pow3( abs( environment ), 2.2 );
			};
			environment *= diffuseLight;
			{
				environment.xyz *= _fa_[30 ].xyz * _fa_[31 ].xyz;
			};
			{
				specularLight.xyz *= _fa_[30 ].xyz * _fa_[31 ].xyz;
			};
			{
				diffuseLight.xyz *= _fa_[32 ].xyz;
			};;;;
			vec3 finalColor = ( specularLight * surfaceSpecular.xyz ) + ( diffuseLight * surfaceDiffuse.xyz ) + ( environment * surfaceSpecular.xyz );
			color.xyz = finalColor;
			float lightFactor = 0.0;
			{
				lightFactor += dot3( light0Dir, globalNormal ) * light0Color.w;
			};
			{
				lightFactor += dot3( light1Dir, globalNormal ) * light1Color.w;
			};
			{
				lightFactor += dot3( light2Dir, globalNormal ) * light2Color.w;
			};
			{
				if( _fa_[12 ].z < 1.0 ) {
					color.w = 0.0;
				} else {
					float tmpLightFactor = saturate( lightFactor );
					vec3 tmpAmbient = ambientLight;
					color.w = saturate( ( 1.0 - ( tmpAmbient.x, tmpAmbient.y, tmpAmbient.z )*(1.0/3.0) )
					- ( saturate( tmpLightFactor - _fa_[33 ].y ) + 0.5 + _fa_[33 ].x ) ) * 0.75;
				}
			};
		};
	} else {
		vec3 specular = vec3( 0.0 );
		{
			{
				globalReflection.w = ( 1.0 - sampleNormal.x ) * 4.0 + _fa_[14 ].x;
			};
			vec3 envColor;
			{
				envColor = texCUBElod( samp_dynamicenvmap, globalReflection ).xyz;;
				envColor = pow3( abs( envColor ), 2.2 );
			};
			specular += envColor;
		};
		{
			float glossPowerMask = saturate( dot( surfaceSpecular.xyz, vec3( 0.299, 0.587, 0.114 ) ) );
			float cosi = max( dot3( globalReflection.xyz, _fa_[34 ].xyz ), 0.0 );
			float glossPower;
			float glossScale;
			if ( globalReflection.w <= 1.0 ) {
				glossPower = mix( _fa_[35 ].x, _fa_[35 ].y, globalReflection.w );
				glossScale = mix( _fa_[36 ].x, _fa_[36 ].y, globalReflection.w );
			} else if ( globalReflection.w <= 2.0 ) {
				float amount = globalReflection.w - 1.0;
				glossPower = mix( _fa_[35 ].y, _fa_[35 ].z, amount );
				glossScale = mix( _fa_[36 ].y, _fa_[36 ].z, amount );
			} else {
				float amount = saturate( globalReflection.w - 2.0 );
				glossPower = mix( _fa_[35 ].z, _fa_[35 ].w, amount );
				glossScale = mix( _fa_[36 ].z, _fa_[36 ].w, amount );
			}
			glossPower *= glossPowerMask;
			float glossIntensity;
			{
				float oneOverPi8 = 0.039808917197;
				float glossPowMax = max( glossPower, 0.000001 );
				glossIntensity = pow( max( cosi, 0.0 ), glossPowMax );
				glossIntensity *= ( glossPowMax + 8.0 ) * oneOverPi8;
				glossIntensity *= glossScale;
			};
			specular += vec3( glossIntensity );
		};
		{
			float primeLightFactor = saturate( dot3( _fa_[34 ], globalNormal ) );
			vec3 primeLight = _fa_[37 ].xyz * primeLightFactor;
			vec3 channelLight = vec3( 0.0 )
			+ max( globalNormal.x * _fa_[24 ].xyz, 0 )
			+ max( -globalNormal.x * _fa_[25 ].xyz, 0 )
			+ max( globalNormal.y * _fa_[26 ].xyz, 0 )
			+ max( -globalNormal.y * _fa_[27 ].xyz, 0 )
			+ max( globalNormal.z * _fa_[28 ].xyz, 0 )
			+ max( -globalNormal.z * _fa_[29 ].xyz, 0 );
			vec3 light = primeLight + channelLight;
			{
				vec3 specularLight = light * specular * surfaceSpecular.xyz;
				vec3 diffuseLight = light;
				{
					specularLight.xyz *= _fa_[30 ].xyz * _fa_[31 ].xyz;
				};
				{
					diffuseLight.xyz *= _fa_[32 ].xyz;
				};;;
				color.xyz = ( diffuseLight * surfaceDiffuse.xyz ) + specularLight;
				{
					color.w = 1.0 - ( saturate( primeLightFactor - _fa_[33 ].y ) + 0.5 + _fa_[33 ].x );
				};
			};
		};
	}
	color.xyz = pow3( color.xyz, 1.0 / 2.2 );
	{
		float k = 1.0 - saturate( abs( dot3( globalNormal.xyz, _fa_[38 ].xyz ) ) );
		float r = ( k - 0.25 ) * 1.333333;
		color.xyz += _fa_[39 ].xyz * vec3( r * r );
	};
	{
		out_FragColor0 = color;
		out_FragColor1 = feedback;
		{
			out_FragColor2.xyz = globalNormal.xyz * vec3( 0.5 ) + vec3( 0.5 );
			out_FragColor2.w = dot( surfaceSpecular.rgb, vec3( 1.0 / 3.0 ) );
		};
	};
}
