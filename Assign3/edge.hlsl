void edgeHighLight_vp(float4 position : POSITION,
                      float3 normal      : NORMAL,
                          
                      out float4 oPosition : POSITION,
                      uniform float4x4 worldViewProj)
{
    oPosition = float4(position.xyz + normal * 0.6f, 1);
    //oPosition = float4(position.x, position.yzw);
    oPosition = mul(worldViewProj, oPosition);
}
void edgeHighLight_fp(out float4 colour : COLOR)
{
	colour = float4(0, 1, 1, 1);

}
// float4 main_fp(in float3 TexelPos : TEXCOORD0) : COLOR
//  {
//      float4 oColor;
 
//      oColor.r = 0.5;
//      oColor.g = 0.9;
//      oColor.b = 0.5;
//      oColor.a = 0.5;
 
//      return oColor;
//  }