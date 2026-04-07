textures/barrel/button
{
	qer_editorimage	textures/h_evil/switch
    {
        map $lightmap
    }
    {
        map textures/h_evil/switch
        blendFunc GL_DST_COLOR GL_ZERO
    }
    {
        animMap 1 textures/h_evil/switch_on textures/h_evil/switch_off
        blendFunc GL_ONE GL_ONE
		glow
    }
}
textures/tarascii/dp_ground
{
	qer_editorimage	textures/imperial/grass_ground
	q3map_material	LongGrass
	q3map_nolightmap
	q3map_onlyvertexlighting
    {
        map textures/imperial/grass_ground
        rgbGen vertex
        tcMod scale 0.75 0.75
    }
    {
        map textures/imperial/grass
            surfaceSprites flattened 24 12 45 400
            ssFademax 3000
            ssFadescale 2.5
            ssVariance 1 1.2
            ssWind 1.2
        alphaFunc GE192
        blendFunc GL_ONE GL_ZERO
        depthWrite
        rgbGen exactVertex
    }
}