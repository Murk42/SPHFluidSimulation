
//array should be sized get_enqueued_local_size(0) * get_num_groups(0) * 2
void kernel scanOnComputeGroups(local uint* temp, global uint* array, uint scale)
{	
	const uint n = min((get_global_size(0) - get_local_size(0) * get_group_id(0)), get_local_size(0)) * 2;
	global uint* ptr = array + get_local_size(0) * get_group_id(0) * 2 * scale;

	int thid = get_local_id(0);
	int offset = 1;	
	
	temp[2 * thid + 0] = ptr[(2 * thid + 1) * scale - 1]; // load input into shared memory
	temp[2 * thid + 1] = ptr[(2 * thid + 2) * scale - 1];	
	
	for (int d = n>>1; d > 0; d >>= 1) // build sum in place up the tree
	{
		barrier(CLK_LOCAL_MEM_FENCE);		
		
		if (thid < d)
		{
			int ai = offset*(2*thid+1)-1;
			int bi = offset*(2*thid+2)-1;			
			
			temp[bi] += temp[ai];
		}		

		offset *= 2;
	}
	
	
	if (thid == 0) 
	{
		temp[n] = temp[n - 1];
		temp[n - 1] = 0;
	} // clear the last element
	
	
	for (int d = 1; d < n; d *= 2) // traverse down tree & build scan
	{
		offset >>= 1;
		barrier(CLK_LOCAL_MEM_FENCE);
		
		if (thid < d)
		{
			int ai = offset*(2*thid+1)-1;
			int bi = offset*(2*thid+2)-1;

			float t = temp[ai];
			temp[ai] = temp[bi];
			temp[bi] += t;			
		}
	}
	
	barrier(CLK_LOCAL_MEM_FENCE);		
	
	ptr[(2 * thid + 1) * scale - 1] = temp[2*thid + 1]; // write results to device memory
	ptr[(2 * thid + 2) * scale - 1] = temp[2*thid + 2];
}

void kernel addToComputeGroupArrays(global uint* arrays, uint scale)
{	
	uint index = (get_group_id(0) + 1) * scale * (get_local_size(0) + 1);
	uint inc = arrays[index - 1];

	
	arrays[index + scale * get_local_id(0)] += inc;
}