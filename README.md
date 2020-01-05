Prototype Vulkan backend for Pixar USD Hydra Graphics Interface (HGI).

If you wish to run the test application, you can download it:
https://github.com/lumonix/hgiVk/releases


> :warning: These are **debug** builds with vulkan validation layers enabled.
> It will run significantly slower than the release builds and will assert on errors.
> There are known bugs / limitations. For example you might experience flickering / tearing.
> There are missing barriers between UI thread reads and hydra thread writes.


The test application focusses on two parts of Hydra.
1. Resource Sync
2. Render Pass Execute

There is a third phase managed by the application:
3. EndFrame

During EndFrame we submit all GPU work recorded and proceed to the next CPU frame.


## Resource Sync ##

When Hydra wants to create, destroy or update prims, such as a mesh, it will do so during Sync.
The sync of many prims runs parallel via tbb parallel_for loops.

HgiVk can parallal record the resource changes immediately and lock free.
It manages this via HgiVkCommandBufferManager (CBM) by ensuring there is a command buffer for each thread.
CBM manages this via thread local storage (TLS).
Each frame it will give each thread exclusive access to a command buffer.

During **EndFrame** the command buffers of each thread are submitted to the queue.

Note that new resources are allocated via AMD's VulkanMemoryAllocator and it may not be lock-free.

Deleting of resources is managed via **HgiVkGarbageCollector**, more on that in EndFrame.


## Render Pass Execute ##

A render pass in Hydra terms is the begin (and end) of one frame of rendering.
Hydra provided a list of render targets to fill ('AOVs').

The basic responsibility is that during execute, for each rprim, we record one or more draw calls.
This could be single thread or multi-threaded. It is entirely up to the RenderDelegate to implement.

In HgiVk we support parallel draw-call recording via **HgiVkParallalCommandEncoder** (PCE).
Similar to resource sync, PCE ensures there is a (draw) command buffer for each thread.

However, vulkan requires that one 'render pass' (a set of attachments + draw calls) begins and ends in one **primary** command buffer. To do parallel command recording vulkan requires us to use **secondary** command buffers.

PCE will interface with HgiVkCommandBufferManager to ensure each draw-call-thread has exclusive access to a secondary command buffer. Again via TLS. When the PCE finishes rendering it 'executes' the secondary command buffers into the primary.


## EndFrame ##

When all sync and execute is completed a call to Hgi::EndFrame is called by the application.
This ends recording on all (parallel) command buffers and submits them to the vulkan queue.

There are three frames that we alternate between (ring buffer).

Frame 0 is where the CPU records new draw calls and resource changes.
Frame 2 is being consumed by the GPU.
Frame 1 is a buffer between CPU and GPU to ensure they are not touching each others changes.

Hydra Sync may choose to destroy (deallocate) a prim at any time without considering if the GPU is still consuming this resource. For that reason we record the desire to destroy a resource into HgiVkGarbageCollector for each frame.
When we are about to re-use a frame we permanently destroy the GPU resources in the garbage collector.
This ensures the GPU is not currently using a resource we are about to destroy.

The garbage collector is lock free by using a vector-per-thread for collecting to-be-destroyed objects.




## References: ##
* https://ourmachinery.com/post/vulkan-command-buffer-management/
* https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
* https://github.com/SaschaWillems/Vulkan
* https://developer.nvidia.com/vulkan-memory-management
* https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
* https://gpuopen.com/vulkan-barriers-explained/
* https://gpuopen.com/wp-content/uploads/2016/03/VulkanFastPaths.pdf
* https://www.jeremyong.com/vulkan/graphics/rendering/2018/11/23/vulkan-synchonization-primer-part-ii/
