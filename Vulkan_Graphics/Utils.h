#pragma once

// Indices (locations) of Queue Families (if they exist at all)
struct QueuefamilyIndices {
	int graphicsFamily = -1;
	
	//check if queue families are valid
	bool isValid()
	{
		return graphicsFamily >= 0;
	}
};


