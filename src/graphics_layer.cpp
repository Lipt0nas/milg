#include <milg.hpp>

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#include "application.hpp"
#include "graphics/sprite_batch.hpp"
#include "graphics/texture.hpp"
#include "graphics/vk_context.hpp"
#include "window.hpp"

using namespace milg;
using namespace milg::graphics;

class GraphicsLayer : public Layer {
public:
    std::shared_ptr<VulkanContext> context = nullptr;

    // This will hold whatever we render in the layer
    std::shared_ptr<Texture> framebuffer = nullptr;

    // Some random texture to draw
    std::shared_ptr<Texture> test_texture = nullptr;

    std::shared_ptr<SpriteBatch> sprite_batch = nullptr;

    void on_attach() override {
        MILG_INFO("Initializing Graphics layer");

        context      = Application::get().context();
        auto &window = Application::get().window();

        TextureCreateInfo texture_info = {
            .format     = VK_FORMAT_R8G8B8A8_UNORM,
            .usage      = VK_IMAGE_USAGE_SAMPLED_BIT,
            .min_filter = VK_FILTER_NEAREST,
            .mag_filter = VK_FILTER_NEAREST,
        };
        {
            auto data          = asset_store::get_asset("light");
            this->test_texture = Texture::load_from_data(context, texture_info, data->get_data(), data->get_size());
        }

        this->framebuffer =
            Texture::create(context,
                            {.format = VK_FORMAT_R8G8B8A8_UNORM,
                             .usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
                            window->width(), window->height());

        // Capacity here is the maximum amount of sprites that can be drawn in one frame, more number
        // allocates more memory, but it's not that much to begin with
        this->sprite_batch = SpriteBatch::create(context, framebuffer->format(), 10000);
    }

    void on_update(float delta) override {
        // Aquire a command buffer from the application, it will be automatically submitted and disposed at the end of
        // the frame
        auto command_buffer = Application::get().aquire_command_buffer();

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        // Begin recording the command buffer and transition the framebuffer to a layout we can render to
        context->device_table().vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Construct an orthographic projection matrix that is the size of the framebuffer and the center is at (0, 0)
        float     half_width  = framebuffer->width() * 0.5f;
        float     half_height = framebuffer->height() * 0.5f;
        glm::mat4 mat         = glm::ortho(-half_width, half_width, -half_height, half_height, -1.0f, 1.0f);

        // Move the center of the projection matrix to the top left corner
        mat = glm::translate(mat, {-half_width, -half_height, 0.0f});

        // Reset the sprite batch, should be done once at the beginning of the frame
        sprite_batch->reset();
        sprite_batch->begin_batch(mat);

        Sprite sprite = {
            .position = {framebuffer->width() / 2, framebuffer->height() / 2},
            .size     = {100.0f, 100.0f},
            .color    = {1.0f, 0.0f, 0.0f, 1.0f},
        };

        sprite_batch->draw_sprite(sprite, test_texture);

        // After drawing, build_batches should be called, this copies over data to the appropriate buffers
        sprite_batch->build_batches(command_buffer);

        // Prepare some Vulkan structs for rendering, only clearValue might be interesting here
        // as this will be the color the screen is cleared with
        const VkExtent2D extent   = {framebuffer->width(), framebuffer->height()};
        const VkRect2D   scissor  = {{0, 0}, extent};
        const VkViewport viewport = {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = (float)extent.width,
            .height   = (float)extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const std::array<VkRenderingAttachmentInfo, 1> rendering_attachment_infos = {
            VkRenderingAttachmentInfo{
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = framebuffer->image_view(),
                .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode        = VK_RESOLVE_MODE_NONE,
                .resolveImageView   = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue         = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
            },
        };

        const VkRenderingInfo rendering_info = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {{0, 0}, extent},
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = static_cast<uint32_t>(rendering_attachment_infos.size()),
            .pColorAttachments    = rendering_attachment_infos.data(),
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr,
        };
        context->device_table().vkCmdBeginRendering(command_buffer, &rendering_info);
        context->device_table().vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        context->device_table().vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        // Prepare rendering state and let sprite batch execute the draw commands
        sprite_batch->render(command_buffer);

        context->device_table().vkCmdEndRendering(command_buffer);

        // After rendering, transition the framebuffer to a layout that can be used to copy from
        framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // And finally blit the framebuffer to the swapchain image that will be presented
        Application::get().swapchain()->blit_to_current_image(command_buffer, framebuffer->handle(),
                                                              {.width = extent.width, .height = extent.height});

        context->device_table().vkEndCommandBuffer(command_buffer);
    }

    void on_event(Event &event) override {
    }

    void on_detach() override {
        MILG_INFO("Tearing down Graphics layer");
    }
};
