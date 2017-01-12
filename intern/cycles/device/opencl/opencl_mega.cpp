/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef WITH_OPENCL

#include "opencl.h"

#include "buffers.h"

#include "kernel_types.h"

#include "util_foreach.h"
#include "util_md5.h"
#include "util_path.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

class OpenCLDeviceMegaKernel : public OpenCLDeviceBase
{
public:
	OpenCLProgram path_trace_program;

	OpenCLDeviceMegaKernel(DeviceInfo& info, Stats &stats, bool background_)
	: OpenCLDeviceBase(info, stats, background_),
	  path_trace_program(this, "megakernel", "kernel.cl", "-D__COMPILE_ONLY_MEGAKERNEL__ ")
	{
	}

	virtual bool show_samples() const {
		return true;
	}

	virtual void load_kernels(const DeviceRequestedFeatures& /*requested_features*/,
	                          vector<OpenCLProgram*> &programs)
	{
		path_trace_program.add_kernel(ustring("path_trace"));
		programs.push_back(&path_trace_program);
	}

	~OpenCLDeviceMegaKernel()
	{
		task_pool.stop();
		path_trace_program.release();
	}

	void path_trace(vector<RenderTile>& rtiles, int sample)
	{
		/* set the sample ranges */
		cl_mem d_sample_ranges = clCreateBuffer(cxContext,
		                                        CL_MEM_READ_WRITE,
		                                        sizeof(SampleRange) * rtiles.size(),
		                                        NULL,
		                                        &ciErr);
		opencl_assert_err(ciErr, "clCreateBuffer");

		cl_kernel ckSetSampleRange = base_program(ustring("set_sample_range"));

		for(int i = 0; i < rtiles.size(); i++) {
			RenderTile& rtile = rtiles[i];

			/* Cast arguments to cl types. */
			cl_int d_range = i;
			cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
			cl_mem d_rng_state = CL_MEM_PTR(rtile.rng_state);
			cl_int d_sample = sample;
			cl_int d_x = rtile.x;
			cl_int d_y = rtile.y;
			cl_int d_w = rtile.w;
			cl_int d_h = rtile.h;
			cl_int d_offset = rtile.offset;
			cl_int d_stride = rtile.stride;

			kernel_set_args(ckSetSampleRange, 0,
			                d_sample_ranges,
			                d_range,
			                d_buffer,
			                d_rng_state,
			                d_sample,
			                d_x,
			                d_y,
			                d_w,
			                d_h,
			                d_offset,
			                d_stride);

			enqueue_kernel(ckSetSampleRange, 1, 1);
		}

		/* Cast arguments to cl types. */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_int d_num_sample_ranges = rtiles.size();

		cl_kernel ckPathTraceKernel = path_trace_program(ustring("path_trace"));

		cl_uint start_arg_index =
			kernel_set_args(ckPathTraceKernel,
			                0,
			                d_data);

#define KERNEL_TEX(type, ttype, name) \
		set_kernel_arg_mem(ckPathTraceKernel, &start_arg_index, #name);
#include "kernel_textures.h"
#undef KERNEL_TEX

		start_arg_index += kernel_set_args(ckPathTraceKernel,
		                                   start_arg_index,
		                                   d_sample_ranges,
		                                   d_num_sample_ranges);

		/* TODO(mai): calculate a reasonable grid size for the device */
		enqueue_kernel(ckPathTraceKernel, 256, 256);

		opencl_assert(clReleaseMemObject(d_sample_ranges));
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::FILM_CONVERT) {
			film_convert(*task, task->buffer, task->rgba_byte, task->rgba_half);
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);
		}
		else if(task->type == DeviceTask::PATH_TRACE) {
			/* TODO(mai): calculate a reasonable grid size for the device */
			RenderWorkRequest work_request = {256*256, 256*256};
			vector<RenderTile> tiles;

			/* Keep rendering tiles until done. */
			while(task->acquire_tiles(this, tiles, work_request)) {
				int start_sample = tiles[0].start_sample;
				int end_sample = tiles[0].start_sample + tiles[0].num_samples;

#ifndef NDEBUG
				foreach(RenderTile& tile, tiles) {
					assert(start_sample == tile.start_sample);
					assert(end_sample == tile.start_sample + tile.num_samples);
				}
#endif

				for(int sample = start_sample; sample < end_sample; sample++) {
					if(task->get_cancel()) {
						if(task->need_finish_queue == false)
							break;
					}

					path_trace(tiles, sample);

					int pixel_samples = 0;
					foreach(RenderTile& tile, tiles) {
						tile.sample = sample + 1;
						pixel_samples += tile.w * tile.h;
					}

					/* TODO(mai): without this we cant see tile updates, however this has a huge impact on
					 * performace. it should be posible to solve this by using a more async style loop
					 */
					clFinish(cqCommandQueue);

					task->update_progress(tiles, pixel_samples);
				}

				/* Complete kernel execution before release tile */
				/* This helps in multi-device render;
				 * The device that reaches the critical-section function
				 * release_tile waits (stalling other devices from entering
				 * release_tile) for all kernels to complete. If device1 (a
				 * slow-render device) reaches release_tile first then it would
				 * stall device2 (a fast-render device) from proceeding to render
				 * next tile.
				 */
				clFinish(cqCommandQueue);

				foreach(RenderTile& tile, tiles) {
					task->release_tile(tile);
				}

				tiles.clear();
			}
		}
	}
};

Device *opencl_create_mega_device(DeviceInfo& info, Stats& stats, bool background)
{
	return new OpenCLDeviceMegaKernel(info, stats, background);
}

CCL_NAMESPACE_END

#endif
