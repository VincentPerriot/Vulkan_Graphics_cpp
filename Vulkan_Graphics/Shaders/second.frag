#version 450

// Color output from subpass 1
layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
// Depth output from subpass 1
layout(input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

layout(location = 0) out vec4 color;

void main()
{
	// Keep as example of subpass use
	//int xHalf = 1366/2;
	//if (gl_FragCoord.x > xHalf)
	//{
	float lowerBound = 0.995;
	float upperBound = 1;

	float depth = subpassLoad(inputDepth).r;
	float depthColorScaled = 1.0f - ((depth - lowerBound) / (upperBound - lowerBound));
	vec4 a = subpassLoad(inputColor);

	color = min(subpassLoad(inputColor).rgba ,vec4( subpassLoad(inputColor).rgb * depthColorScaled, 1.0f));
//	}
//	else 
//	{
//	color = subpassLoad(inputColor).rgba;
//	}
}
