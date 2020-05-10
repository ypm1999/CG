Shader "Custom/Gooch"
{
	Properties{
		_Color("Color", Color) = (1, 1, 1, 1)
		_Specular("Specular", Color) = (1, 1, 1, 1)

		_Warm_Color("Warm Color",Color) = (0.3, 0.3, 0, 1)
		_Cool_Color("Cool Color",Color) = (0, 0, 0.55, 1)
	}
	SubShader{
		Tags { "RenderType" = "Opaque" "Queue" = "Geometry"}


		Pass {
			Tags { "LightMode" = "ForwardAdd" }

			CGPROGRAM
			#pragma multi_compile_fwdbase
			#pragma vertex vert
			#pragma fragment frag

			#include "Lighting.cginc"

			fixed4 _Color;
			fixed4 _Specular;
			float _Shine;

			float _a;
			float _b;
			fixed4 _Warm_Color;
			fixed4 _Cool_Color;

			struct a2v {
				float4 vertex:POSITION;
				float3 normal:NORMAL;
			};
			struct v2f {
				float4 pos:SV_POSITION;
				float3 worldPos:TEXCOORD1;
				float3 worldnormal:TEXCOORD2;
			};

			v2f vert(a2v v) {
				v2f o;
				o.pos = UnityObjectToClipPos(v.vertex);
				o.worldnormal = UnityObjectToWorldNormal(v.normal);
				o.worldPos = mul(unity_ObjectToWorld, v.vertex).xyz;
				return o;
			}

			fixed4 frag(v2f o) :SV_TARGET{
				float3 LightDir = normalize(UnityWorldSpaceLightDir(o.worldPos));
				float3 ViewDir = normalize(UnityWorldSpaceViewDir(o.worldPos));
				float t = 0.5*(dot(o.worldnormal,LightDir) + 1);
				float3 r =
				2 * dot(o.worldnormal,LightDir)*o.worldnormal - LightDir;
				float s = saturate(100 * dot(r,ViewDir) - 97);

				fixed4 c = s * _Specular + (1 - s)*(t*(_Warm_Color + 0.25*_Color) + 
							(1 - t)*(_Cool_Color + 0.25*_Color));

				return c;
			}
			ENDCG
		}
	}
	FallBack "Diffuse"
}
