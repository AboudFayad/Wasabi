#include "WForwardRenderer.h"

static VkCommandBuffer g_CreateCommandBuffer(VkDevice device, VkCommandPool pool) {
	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkTools::initializers::commandBufferAllocateInfo(
			pool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);

	VkCommandBuffer buff;
	VkResult err = vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &buff);
	if (err != VK_SUCCESS)
		return VK_NULL_HANDLE;

	return buff;
}
static void g_FreeCommandBuffer(VkDevice device, VkCommandPool pool, VkCommandBuffer* buf) {
	if (*buf != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(device, pool, 1, buf);
		*buf = VK_NULL_HANDLE;
	}
}

WForwardRenderer::WForwardRenderer(Wasabi* app) : WRenderer(app) {
	m_swapchainInitialized = false;
	m_clearColor = { { 0.425f, 0.425f, 0.425f, 1.0f } };
	m_setupCmdBuffer = m_renderCmdBuffer = VK_NULL_HANDLE;
	m_cmdPool = VK_NULL_HANDLE;
	m_renderPass = VK_NULL_HANDLE;
	m_pipelineCache = VK_NULL_HANDLE;
	m_descriptorPool = VK_NULL_HANDLE;
	m_colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
}
WForwardRenderer::~WForwardRenderer() {
	Cleanup();
}

WError WForwardRenderer::Initiailize() {
	Cleanup();

	m_device = m_app->GetVulkanDevice();
	m_queue = m_app->GetVulkanGraphicsQeueue();
	VkPhysicalDevice physDev = m_app->GetVulkanPhysicalDevice();

	// Find a suitable depth format
	VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(physDev, &m_depthFormat);
	if (!m_depthFormat)
		return WError(W_HARDWARENOTSUPPORTED);

	m_swapChain.connect(m_app->GetVulkanInstance(), physDev, m_device);
	if (!m_swapChain.initSurface(
		m_app->WindowComponent->GetPlatformHandle(),
		m_app->WindowComponent->GetWindowHandle()))
		return WError(W_UNABLETOCREATESWAPCHAIN);
	m_swapchainInitialized = true;

	VkResult err = _SetupSemaphores();
	if (err != VK_SUCCESS)
		return WError(W_OUTOFMEMORY);

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = m_swapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	err = vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &m_cmdPool);
	if (err != VK_SUCCESS)
		return WError(W_OUTOFMEMORY);

	m_renderCmdBuffer = g_CreateCommandBuffer(m_device, m_cmdPool);
	if (m_renderCmdBuffer == VK_NULL_HANDLE)
		return WError(W_OUTOFMEMORY);

	//
	// Beginning of setup commands session
	//
	if (_BeginSetupCommands() != VK_SUCCESS)
		return WError(W_OUTOFMEMORY);

	err = _SetupSwapchain();
	if (err != VK_SUCCESS)
		return WError(W_OUTOFMEMORY);

	// create pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	err = vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache);
	assert(!err);

	// Create descriptor pool
	// We need to tell the API the number of max. requested descriptors per type
	VkDescriptorPoolSize typeCounts[1];
	// This example only uses one descriptor type (uniform buffer) and only
	// requests one descriptor of this type
	typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	typeCounts[0].descriptorCount = 1;
	// For additional types you need to add new entries in the type count list
	// E.g. for two combined image samplers :
	// typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// typeCounts[1].descriptorCount = 2;

	// Create the global descriptor pool
	// All descriptors used in this example are allocated from this pool
	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = NULL;
	descriptorPoolInfo.poolSizeCount = 1;
	descriptorPoolInfo.pPoolSizes = typeCounts;
	// Set the max. number of sets that can be requested
	// Requesting descriptors beyond maxSets will result in an error
	descriptorPoolInfo.maxSets = 1;

	VkResult vkRes = vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool);
	assert(!vkRes);

	if (_EndSetupCommands() != VK_SUCCESS)
		return WError(W_OUTOFMEMORY);
	//
	// ===================================
	//

	return WError(W_SUCCEEDED);
}

WError WForwardRenderer::Render() {
	vkDeviceWaitIdle(m_device);

	// Get next image in the swap chain (back/front buffer)
	uint32_t currentBuffer;
	VkResult err = m_swapChain.acquireNextImage(m_semaphores.presentComplete, &currentBuffer);
	assert(!err);

	//submitPostPresentBarrier(m_swapChain.buffers[currentBuffer].image);

	VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = m_clearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vkTools::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = m_width;
	renderPassBeginInfo.renderArea.extent.height = m_height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	// Set target frame buffer
	renderPassBeginInfo.framebuffer = m_frameBuffers[currentBuffer];

	// Bind descriptor sets describing shader binding points
	err = vkResetCommandBuffer(m_renderCmdBuffer, 0);
	assert(!err);

	err = vkBeginCommandBuffer(m_renderCmdBuffer, &cmdBufInfo);
	assert(!err);

	vkCmdBeginRenderPass(m_renderCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = vkTools::initializers::viewport(
		(float)m_width,
		(float)m_height,
		0.0f,
		1.0f);
	vkCmdSetViewport(m_renderCmdBuffer, 0, 1, &viewport);

	VkRect2D scissor = vkTools::initializers::rect2D(
		m_width,
		m_height,
		0,
		0);
	vkCmdSetScissor(m_renderCmdBuffer, 0, 1, &scissor);

	m_app->ObjectManager->Render();

	vkCmdEndRenderPass(m_renderCmdBuffer);

	err = vkEndCommandBuffer(m_renderCmdBuffer);
	assert(!err);

	// Command buffer to be sumitted to the queue
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo submitInfo = vkTools::initializers::submitInfo();
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = &m_semaphores.presentComplete;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = &m_semaphores.renderComplete;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_renderCmdBuffer;

	// Submit to queue
	err = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
	assert(!err);

	//submitPrePresentBarrier(m_swapChain.buffers[currentBuffer].image);

	err = m_swapChain.queuePresent(m_queue, currentBuffer, m_semaphores.renderComplete);
	assert(!err);

	vkDeviceWaitIdle(m_device);

	return WError(W_SUCCEEDED);
}

void WForwardRenderer::Cleanup() {
	if (m_descriptorPool)
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
	if (m_renderPass)
		vkDestroyRenderPass(m_device, m_renderPass, nullptr);
	if (m_pipelineCache)
		vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);

	if (m_semaphores.presentComplete)
		vkDestroySemaphore(m_device, m_semaphores.presentComplete, nullptr);
	if (m_semaphores.renderComplete)
		vkDestroySemaphore(m_device, m_semaphores.renderComplete, nullptr);

	g_FreeCommandBuffer(m_device, m_cmdPool, &m_setupCmdBuffer);
	g_FreeCommandBuffer(m_device, m_cmdPool, &m_renderCmdBuffer);
	if (m_cmdPool)
		vkDestroyCommandPool(m_device, m_cmdPool, nullptr);

	for (uint32_t i = 0; i < m_frameBuffers.size(); i++)
		vkDestroyFramebuffer(m_device, m_frameBuffers[i], nullptr);
	m_frameBuffers.clear();
	if (m_swapchainInitialized)
		m_swapChain.cleanup();
	m_swapchainInitialized = false;
}

void WForwardRenderer::SetClearColor(WColor  col) {
	m_clearColor = { { col.r, col.g, col.b, col.a } };
}

VkResult WForwardRenderer::_BeginSetupCommands() {
	g_FreeCommandBuffer(m_device, m_cmdPool, &m_setupCmdBuffer);

	m_setupCmdBuffer = g_CreateCommandBuffer(m_device, m_cmdPool);
	if (m_setupCmdBuffer == VK_NULL_HANDLE)
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkResult err = vkBeginCommandBuffer(m_setupCmdBuffer, &cmdBufInfo);
	if (err != VK_SUCCESS) {
		g_FreeCommandBuffer(m_device, m_cmdPool, &m_setupCmdBuffer);
		return err;
	}

	return err;
}

VkResult WForwardRenderer::_EndSetupCommands() {
	VkResult err;

	if (m_setupCmdBuffer == VK_NULL_HANDLE)
		return VK_SUCCESS;

	err = vkEndCommandBuffer(m_setupCmdBuffer);
	if (err != VK_SUCCESS)
		return err;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_setupCmdBuffer;

	err = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
	if (err != VK_SUCCESS)
		return err;

	err = vkQueueWaitIdle(m_queue);
	if (err != VK_SUCCESS)
		return err;

	g_FreeCommandBuffer(m_device, m_cmdPool, &m_setupCmdBuffer);

	return err;
}

VkResult WForwardRenderer::_SetupSemaphores() {
	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkTools::initializers::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	VkResult err = vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.presentComplete);
	if (err != VK_SUCCESS)
		return err;
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	err = vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.renderComplete);
	if (err != VK_SUCCESS)
		return err;

	return VK_SUCCESS;
}

VkResult WForwardRenderer::_SetupSwapchain() {
	//
	// Setup the swap chain
	//
	m_width = m_app->WindowComponent->GetWindowWidth();
	m_height = m_app->WindowComponent->GetWindowHeight();
	m_swapChain.create(m_setupCmdBuffer, &m_width, &m_height);

	//
	// Create the render pass
	//
	VkAttachmentDescription attachments[2] = {};

	// Color attachment
	attachments[0].format = m_colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment
	attachments[1].format = m_depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.flags = 0;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = &depthReference;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = NULL;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = NULL;

	VkResult err;

	err = vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass);
	assert(!err);

	//
	// Create the depth-stencil buffer
	//
	VkImageCreateInfo image = {};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.pNext = NULL;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = m_depthFormat;
	image.extent = { m_width, m_height, 1 };
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image.flags = 0;

	VkMemoryAllocateInfo mem_alloc = {};
	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.pNext = NULL;
	mem_alloc.allocationSize = 0;
	mem_alloc.memoryTypeIndex = 0;

	VkImageViewCreateInfo depthStencilView = {};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.pNext = NULL;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = m_depthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	VkMemoryRequirements memReqs;

	err = vkCreateImage(m_device, &image, nullptr, &m_depthStencil.image);
	assert(!err);
	vkGetImageMemoryRequirements(m_device, m_depthStencil.image, &memReqs);
	mem_alloc.allocationSize = memReqs.size;
	m_app->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex);
	err = vkAllocateMemory(m_device, &mem_alloc, nullptr, &m_depthStencil.mem);
	assert(!err);

	err = vkBindImageMemory(m_device, m_depthStencil.image, m_depthStencil.mem, 0);
	assert(!err);
	vkTools::setImageLayout(
		m_setupCmdBuffer,
		m_depthStencil.image,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	depthStencilView.image = m_depthStencil.image;
	err = vkCreateImageView(m_device, &depthStencilView, nullptr, &m_depthStencil.view);
	assert(!err);

	//
	// Create the frame buffers
	//
	VkImageView imageViews[2];

	// Depth/Stencil attachment is the same for all frame buffers
	imageViews[1] = m_depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = m_renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = imageViews;
	frameBufferCreateInfo.width = m_width;
	frameBufferCreateInfo.height = m_height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	m_frameBuffers.resize(m_swapChain.imageCount);
	for (uint32_t i = 0; i < m_frameBuffers.size(); i++) {
		imageViews[0] = m_swapChain.buffers[i].view;
		VkResult err = vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &m_frameBuffers[i]);
		assert(!err);
	}

	return VK_SUCCESS;
}

VkQueue WForwardRenderer::GetQueue() const {
	return m_queue;
}
VkRenderPass WForwardRenderer::GetRenderPass() const {
	return m_renderPass;
}
VkPipelineCache WForwardRenderer::GetPipelineCache() const {
	return m_pipelineCache;
}
VkCommandBuffer WForwardRenderer::GetCommnadBuffer() const {
	return m_renderCmdBuffer;
}
VkCommandPool WForwardRenderer::GetCommandPool() const {
	return m_cmdPool;
}
VkDescriptorPool WForwardRenderer::GetDescriptorPool() const {
	return m_descriptorPool;
}
