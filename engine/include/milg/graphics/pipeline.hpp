#pragma once

#include <milg/graphics/buffer.hpp>
#include <milg/graphics/texture.hpp>
#include <milg/graphics/vk_context.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace milg::graphics {
    struct PipelineOutputDescription {
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width  = 0;
        uint32_t height = 0;
    };

    struct Pipeline {
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

        VkPipeline       pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout   = VK_NULL_HANDLE;

        VkDescriptorSetLayout        set_layout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> sets;

        VkQueryPool query_pool = VK_NULL_HANDLE;

        uint32_t query_index    = 0;
        float    execution_time = 0;
        uint32_t dispatch_count = 0;

        std::vector<std::shared_ptr<Texture>> output_buffers;

        void bind_texture(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                          uint32_t binding, const std::shared_ptr<Texture> &texture);

        void bind_buffer(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                         uint32_t binding, const std::shared_ptr<Buffer> &buffer);

        void begin(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                   uint32_t push_constant_size = 0, const void *push_constant_data = nullptr);
        void end(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer);

        void set_push_constants(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                                uint32_t size, const void *data);

        void dispatch(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer, uint32_t size_x,
                      uint32_t size_y, uint32_t size_z);

        void rebind_descriptor_set(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer);

        void allocate_new_set(const std::shared_ptr<VulkanContext> &context);
    };

    class PipelineFactory {
    public:
        static std::shared_ptr<PipelineFactory> create(const std::shared_ptr<VulkanContext> &context);

        ~PipelineFactory();

        Pipeline *create_compute_pipeline(const std::string &name, const std::string &shader_id,
                                          const std::vector<PipelineOutputDescription> &output_descriptions,
                                          uint32_t texture_input_count, uint32_t buffer_input_count,
                                          uint32_t push_constant_size = 0);
        void      begin_frame(VkCommandBuffer command_buffer);
        void      end_frame(VkCommandBuffer command_buffer);

        Pipeline *get_pipeline(const std::string &name);

        const std::map<std::string, Pipeline> &get_pipelines() const;

        float pre_execution_time() const;

    private:
        std::shared_ptr<VulkanContext> m_context = nullptr;

        VkDescriptorPool                m_global_descriptor_pool = VK_NULL_HANDLE;
        std::map<std::string, Pipeline> m_pipelines;
        std::array<VkQueryPool, 2>      m_query_pools = {VK_NULL_HANDLE, VK_NULL_HANDLE};

        float    m_pre_execution_time = 0;
        uint32_t m_frame_index        = true;

        PipelineFactory() = default;
    };
} // namespace milg::graphics
