int run_young_gpu_bfs (DefNewGeneration* _g)
{
  ReferenceGraphNode::toGPUReferenceGraph ();
 int marked, unmarked;
  HeapWord* boundary = _g->reserved().end();
  thread_f (NULL);

  cl_platform_id platforms[100];
  cl_uint platforms_n = 0;
  cl_int _err;
  (clGetPlatformIDs(100, platforms, &platforms_n));
  printf ("[[[Starting OpenCL]]]\n");
  //printf("=== %d OpenCL platform(s) found: ===\n", platforms_n);
  if (platforms_n == 0)
    return 1;

  cl_device_id devices[100];
  cl_uint devices_n = 0;
  clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 100, devices, &devices_n);

  //printf("=== %d OpenCL device(s) found on platform:\n", platforms_n);

  if (devices_n == 0)
    return 1;

  char *program_source;
  ifstream gpu_mark_cl;
  
  gpu_mark_cl.open ("/home/abhi/OpenJDK/openjdk/hotspot/src/share/vm/memory/gpu_mark_young.cl");
  
  if (!gpu_mark_cl.is_open ())
  {
    std::cout<<"Cannot open gpu_mark.c"<<std::endl;
    return 1;
  }
  
  gpu_mark_cl.seekg(0, std::ios::end);    // go to the end
  int length = gpu_mark_cl.tellg();           // report location (this is the length)
  gpu_mark_cl.seekg(0, std::ios::beg);
  program_source = new char [length+1];
  gpu_mark_cl.read(program_source, length);       // read the whole file into the buffer
  program_source[length] = '\0';
  
  
  gpu_mark_cl.close ();
  
  const char* _program_source = program_source;
  cl_context context;
  
  context = (clCreateContext(NULL, 1, devices, &pfn_notify, NULL, &_err));

  cl_program program;
  
  program = (clCreateProgramWithSource(context, 1, &_program_source , NULL, &_err));

  if (clBuildProgram(program, 1, devices, NULL, NULL, NULL) != CL_SUCCESS) {
    char buffer[20240];
    clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, NULL);
    std::cout<<"CL Compilation failed:"<< std::endl<< buffer;
    abort();
  }
 // clUnloadCompiler();
    
    /*int marked = 0, unmarked = 0;
    
    for (uint64_t i = 0; i < ReferenceGraphNode::mapOOPDescGraph.size (); i++)
    {
      if (GPUReferenceGraph::array_gpu_graph_nodes[i].marked == 1)
         marked++;
      else
        unmarked++;
    }
    
    std::cout<<"EARLIER MARKED " << marked << " " << unmarked << " " << sizeof (GPUReferenceGraphNode) << endl;
    */
    


    cl_mem input_buffer1;
    input_buffer1 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                sizeof(GPUReferenceGraphNode)*ReferenceGraphNode::mapOOPDescGraph.size (), 
                                GPUReferenceGraph::array_gpu_graph_nodes, &_err));

    cl_mem input_buffer2;
    input_buffer2 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                 sizeof(int)*GPUReferenceGraph::adjacent_vertices.size (), 
                                 &GPUReferenceGraph::adjacent_vertices[0], &_err));
    
    int n_root_nodes;
    
    n_root_nodes = ReferenceGraphNode::root->_children.size ();
    int root_nodes [n_root_nodes];
    
    map<oopDesc*, ReferenceGraphNode*>::iterator it;
  
  
  /*for (it = ReferenceGraphNode::mapOOPDescGraph.begin (); it != ReferenceGraphNode::mapOOPDescGraph.end(); it++)
  {
    std::cout<< it->second->index<<std::endl;
  }*/
    int i = 0;
    for (set<ReferenceGraphNode*>::iterator it = ReferenceGraphNode::root->_children.begin (); 
         it != ReferenceGraphNode::root->_children.end (); ++it)
    {
      root_nodes [i] = (*it)->index;
 i++;
      //std::cout<< root_nodes[i] << std::endl;
    }

    
    
    /*for (int i = 0; i < n_root_nodes; i++)
    {
      queue<int> _queue;
      int root = ReferenceGraphNode::root->_children[i]->index;
      
      _queue.push (root);
      
      while (_queue.size () != 0)
      {
        int p = _queue.front ();
        
        _queue.pop ();
        
        if (GPUReferenceGraph::array_gpu_graph_nodes[p].marked != 1)
        {
          GPUReferenceGraph::array_gpu_graph_nodes[p].marked = 1;
          
          if (GPUReferenceGraph::array_gpu_graph_nodes[p].first_index ==
              GPUReferenceGraph::array_gpu_graph_nodes[p].last_index)
            continue;

          for (int l = GPUReferenceGraph::array_gpu_graph_nodes[p].first_index; 
               l < GPUReferenceGraph::array_gpu_graph_nodes[p].last_index; l++)
          {
            int vertex = GPUReferenceGraph::adjacent_vertices [l];
            
            if (vertex != -1)
              _queue.push (vertex);
          }
        }
      }
    }*/
    
    
    int max_id, min_id;
    cl_mem input_buffer3;
    input_buffer3 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                 sizeof(int)*n_root_nodes, 
                                 &root_nodes[0], &_err));
    int* heads = new int[n_root_nodes];

    int* tails = new int[n_root_nodes];
    
    for (int kk = 0; kk < n_root_nodes; kk++) heads[kk]=tails[kk] = 0;
    cl_mem input_buffer6;
    input_buffer6 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                sizeof(int)*n_root_nodes, 
                                &tails[0], &_err));

    int total_youngs = 0;
    int* youngs = new int [ReferenceGraphNode::n_young_objects];
        std::cout<<"young "<< ReferenceGraphNode::n_young_objects << " total " << ReferenceGraphNode::mapOOPDescGraph.size ()<<std::endl;
    cl_mem input_buffer7;
    input_buffer7 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                sizeof(int), 
                                &total_youngs, &_err));

    cl_mem input_buffer8;
    input_buffer8 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                sizeof(int)*ReferenceGraphNode::n_young_objects, 
                                &youngs[0], &_err));


    int* nodes = new int [ReferenceGraphNode::mapOOPDescGraph.size ()];
    int* nodes2 = new int [ReferenceGraphNode::mapOOPDescGraph.size ()];

    for (uint64_t i = 0; i < ReferenceGraphNode::mapOOPDescGraph.size (); i++)
    {
      nodes[i]= -1;
      nodes2[i] = -1;
    }

    cl_mem input_buffer5;
    input_buffer5 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                sizeof(int)*n_root_nodes, 
                                &nodes2[0], &_err));
  
    cl_mem input_buffer4;
    input_buffer4 = CL_CHECK_ERR(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                 sizeof(nodes), 
                                 &nodes[0], &_err));
                                  
    cl_command_queue queue;
     
    queue = (clCreateCommandQueue(context, devices[0], 0, &_err));
    int factor = 64;
    int n = n_root_nodes/factor;
    n = (n+1)*factor;
    cout<< n << " " << factor << " F F "<< n_root_nodes << endl;
    unsigned long __boundary = (unsigned long)boundary;
//std::cout<<"78111"<<std::endl;
    clock_t ct1, ct2;
    ct1 = clock ();
        
    //for (int i = 0; i < n_root_nodes; i++)
    {
      mark_phase (GPUReferenceGraph::array_gpu_graph_nodes, &GPUReferenceGraph::adjacent_vertices[0], &root_nodes[0], &nodes[0], n_root_nodes, &total_youngs, &youngs[0],__boundary, &nodes[0]);
    }
    

    ct2 = clock ();
    std::cout<< "CPU REF GRAPH " << (((float)ct2 - (float)ct1)/1000000.0)*1000 <<std::endl;

    //pthread_t threads [n_root_nodes];
    /*ct1 = clock ();
    for (int i = 0; i < n_root_nodes; i++)
    {
      pthread_create (&threads[i], NULL, thread_f, NULL);
    }
    
    for (int i = 0; i < n_root_nodes; i++)
      pthread_join (threads[i], NULL);
    ct2 = clock ();
    std::cout<< "CPU REF PARALLEL GRAPH " << (((float)ct2 - (float)ct1) / 1000000.0 ) * 1000 <<std::endl;
*/

    for (uint64_t i = 0; i < ReferenceGraphNode::mapOOPDescGraph.size (); i++)
    {
      nodes[i]= -1;
      nodes2[i] = -1;
    }
    total_youngs = 0;

    marked = 0;
    unmarked = 0;
    //std::cout<<"6767111"<<std::endl;
    for (uint64_t i = 0; i < ReferenceGraphNode::mapOOPDescGraph.size (); i++)
    {
      if (GPUReferenceGraph::array_gpu_graph_nodes[i].marked == 1)
      {
        GPUReferenceGraph::array_gpu_graph_nodes[i].marked = 0;
         marked++;
      }
      else
        unmarked++;
    }
    //std::cout<<" MAffgfgfgRKED " << marked << "   " << unmarked<<std::endl;

  //std::cout<<"DDDD"<<std::endl;
    std::cout<< "Total Size "<< sizeof(GPUReferenceGraphNode)*ReferenceGraphNode::mapOOPDescGraph.size ()+ sizeof(int)*GPUReferenceGraph::adjacent_vertices.size () + sizeof(int)*n_root_nodes + sizeof(int)*ReferenceGraphNode::mapOOPDescGraph.size () << " SIZE "<<std::endl;

    //cout<<"FFF " << n<< " " << n_root_nodes << endl;

    ct1 = clock ();
    clEnqueueWriteBuffer(queue, input_buffer1, CL_TRUE, 0, 
                         sizeof(GPUReferenceGraphNode)*ReferenceGraphNode::mapOOPDescGraph.size (), 
                         GPUReferenceGraph::array_gpu_graph_nodes, 0, NULL, NULL);

    clEnqueueWriteBuffer(queue, input_buffer2, CL_TRUE, 0, 
                         sizeof(int)*GPUReferenceGraph::adjacent_vertices.size (), 
                         &GPUReferenceGraph::adjacent_vertices[0], 0, NULL, NULL);

    clEnqueueWriteBuffer(queue, input_buffer3, CL_TRUE, 0, 
                         sizeof(int)*n_root_nodes, 
                         &root_nodes[0], 0, NULL, NULL);

    clEnqueueWriteBuffer(queue, input_buffer4, CL_TRUE, 0, 
                         sizeof(int)*ReferenceGraphNode::mapOOPDescGraph.size (), 
                         &nodes[0], 0, NULL, NULL);
    
    clEnqueueWriteBuffer(queue, input_buffer5, CL_TRUE, 0, 
                         sizeof(int)*n_root_nodes, 
                         &nodes2[0], 0, NULL, NULL);
                         
    clEnqueueWriteBuffer(queue, input_buffer6, CL_TRUE, 0, 
                         sizeof(int)*n_root_nodes, 
                         &tails[0], 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, input_buffer7, CL_TRUE, 0, 
                         sizeof(int), 
                                &total_youngs, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, input_buffer8, CL_TRUE, 0, 
                         sizeof(int)*ReferenceGraphNode::n_young_objects, 
                                &youngs[0], 0, NULL, NULL);
   ct2 = clock ();
    std::cout<< "GPU COPY " << (((float)ct2 - (float)ct1) / 1000000.0 ) * 1000 <<std::endl;
   
    i = ReferenceGraphNode::mapOOPDescGraph.size ();
cl_kernel kernel;
    //std::cout<<"n_young_objects " << ReferenceGraphNode::n_young_objects << " __boundary " << __boundary << std::endl;
    ct1 = clock ();
    kernel = (clCreateKernel(program, "mark_phase", &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(input_buffer1), &input_buffer1));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(input_buffer2), &input_buffer2));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(input_buffer3), &input_buffer3));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(input_buffer4), &input_buffer4));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(n_root_nodes), &n_root_nodes));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(input_buffer7), &input_buffer7));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(input_buffer8), &input_buffer8));
    CL_CHECK(clSetKernelArg(kernel, 7, sizeof(__boundary), &__boundary));
    CL_CHECK(clSetKernelArg(kernel, 8, sizeof(input_buffer5), &input_buffer5));
    CL_CHECK(clSetKernelArg(kernel, 9, sizeof(n_root_nodes), &i));

//std::cout<<"DDDD2222"<<std::endl;
    cl_event kernel_completion;
    size_t global_work_size[1] = {n};
    size_t local_work_size[1] = {factor};
     CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_work_size, local_work_size, 0, NULL, &kernel_completion));
    //std::cout<<"ssdsdsdsd"<<std::endl;
    CL_CHECK(clWaitForEvents(1, &kernel_completion));
    //std::cout<<"LLLL"<<std::endl;
        (clReleaseEvent(kernel_completion));
        
       //std::cout<<"KKKKK"<<std::endl;
    clFinish (queue);
     
    /*clEnqueueReadBuffer(queue, input_buffer1, CL_TRUE, 0, 
                         sizeof(GPUReferenceGraphNode)*ReferenceGraphNode::mapOOPDescGraph.size (), 
                         GPUReferenceGraph::array_gpu_graph_nodes, 0, NULL, NULL);
    */
    
    ct2 = clock ();
    std::cout<< "GPU " << (((float)ct2 - (float)ct1) / 1000000.0 ) * 1000 <<std::endl;
clEnqueueReadBuffer(queue, input_buffer7, CL_TRUE, 0, 
                         sizeof(int), 
                         &total_youngs, 0, NULL, NULL);    
std::cout<<" YOUNG OBJECTS MOVING TO OLDER " << total_youngs << std::endl;
    /*marked = 0;
     unmarked = 0;
    
    for (uint64_t i = 0; i < ReferenceGraphNode::mapOOPDescGraph.size (); i++)
    {
      if (GPUReferenceGraph::array_gpu_graph_nodes[i].marked == 1)
         marked++;
      else
        unmarked++;
    }
    
    std::cout<< "GPU MARKED " << marked << "   " << unmarked << std::endl;
    */
    printf ("[[[OpenCL Done]]]\n");
delete[] nodes;
delete[] nodes2;
delete[]heads;
delete[] tails;
delete[] youngs;
    (clReleaseMemObject(input_buffer1));
    (clReleaseMemObject(input_buffer2));
    (clReleaseMemObject(input_buffer3));

    (clReleaseKernel(kernel));
    (clReleaseProgram(program));
    (clReleaseContext(context));
    
    return 0;
}
