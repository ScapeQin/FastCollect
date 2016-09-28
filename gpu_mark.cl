#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

#define max_local_queue_size 128
#define max_local_queue_size_power 7
#define GlobalToLocal -2

struct node
{
  int index;
  int first_index;
  int last_index;
  int marked;
  //unsigned long address;
};

typedef struct node Node;

struct Queue
{
	__global int* global_queue;
	__local int* local_queue;
	int local_queue_size;
	int head;
  int tail;
	int global_queue_tail;
	int local_to_global;
	bool head_in_local;
	bool tail_in_local;
};

typedef struct Queue Queue;

inline void queue_init (Queue* queue, __global int* global_queue, __local int* local_queue)
{
	queue->local_queue = local_queue;
	queue->global_queue = global_queue;
	queue->local_queue_size = 0;
	queue->head = 0;
	queue->tail = 0;
	queue->local_to_global = -1;
	queue->head_in_local = true;
	queue->tail_in_local = true;
}

void enqueue (Queue* queue, int data)
{
	if (queue->tail_in_local && queue->tail < max_local_queue_size)
	{
		queue->tail_in_local = true;
		queue->local_queue[queue->tail] = data;
		queue->tail++;
		queue->local_queue_size++;
	}
	else
	{
	  if (queue->tail_in_local && queue->tail == max_local_queue_size)
	  {
	  	queue->tail_in_local = false;
	  	queue->tail = data;
	  	queue->local_to_global = data;
	  }
	  else if (queue->tail != data && queue->global_queue[data] == -1)
  	{
  	  if (queue->head == -1)
  		{
  			queue->tail = data;
  		  queue->head = data;
			}
			else if (queue->local_queue_size == 0)
  	  {
  	    queue->global_queue [queue->tail] = GlobalToLocal;
  	    queue->tail = 0;
  	    queue->tail_in_local = true;
  	    queue->local_queue [0] = data;
  	    queue->local_queue_size = 1;
  	  }
  		else
  		{
  			queue->global_queue[queue->tail] = data;
  		  queue->tail = data;
  		}
  	}
  }
}

int dequeue (Queue* queue)
{
	if (queue->head_in_local)
	{
		int i = queue->local_queue[queue->head];
		
		if (queue->head >= max_local_queue_size - 1)
		{
			queue->head = queue->local_to_global;
			queue->head_in_local = false;
		}
		else
		{
			queue->head++;
			queue->head_in_local = true;
		}
		
		queue->local_queue_size -= 1;
		
		return i;
	}
	else
	{
	  int i;
  	int j;
  
  	j = queue->head;
  	i = queue->global_queue [queue->head];

  	if (i == GlobalToLocal)
  	{
  	  queue->head = 0;
  	  queue->head_in_local = true;
  	  queue->global_queue[queue->head] = -1;
  	}
  	else
  	{
    	queue->global_queue[queue->head] = -1;
    	queue->head = i;
    }

	  return j;
	}
}

inline int is_empty (Queue* queue)
{
  if (queue->head == -1)
    return 1;

  if (queue->local_queue_size == 0 && queue->head == queue->tail &&
  	  queue->head < max_local_queue_size)
  	return 1;

  return 0;
}

__kernel void mark_phase (__global Node* nodes, __global int* adjacent_vertices, __global int* roots, __global int* queue, int n_root_nodes, __global int* marked)
{
  int id = get_global_id (0);
  int _root = roots[id];
  int local_work_size = get_local_size (0);
  __local int _local_queue[8192];
  int local_id = get_local_id (0);
  __local int* local_queue = _local_queue + (local_id<<max_local_queue_size_power);
	
  if (id >= n_root_nodes)
    return;

  Queue hybrid_queue;
  queue_init (&hybrid_queue, queue, local_queue);

  enqueue (&hybrid_queue, _root);
  nodes[_root].marked = 1;

  while (1)
  {
    int i;
    int j;
 
    if (is_empty (&hybrid_queue))
    {
      for (i = work_steal_start; i < work_steal_length + work_steal_start; i++)
      {
        if (queue[i] != -1)
        {
          work_steal_start = i + 1;
          enqueue (&hybrid_queue, i);
          //printf("|stealing %d %d |", i, id);
          break;
        }
      }
      
      if (is_empty (&hybrid_queue))
        break;
    }
    
    j = dequeue (&hybrid_queue);
    
    for (i = nodes[j].first_index; i < nodes[j].last_index; i++)
    {
      int vertex = adjacent_vertices [i];

      if (vertex == -1)
        continue;

      if (nodes [vertex].marked == 0)
      {
        if (atomic_cmpxchg (&nodes [vertex].marked, 0, 1) == 0)
        {
          enqueue (&hybrid_queue, vertex);            
        }
      }
    }
  }
}
