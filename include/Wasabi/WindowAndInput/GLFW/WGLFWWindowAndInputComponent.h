/** @file WWindowsWindowAndInputComponent.h
 *  @brief Window/input component using GLFW, supports Windows, Linux and Mac
 *
 *  @author Hasan Al-Jawaheri (hbj)
 *  @bug No known bugs.
 */

#pragma once

#include "WindowAndInput/WWindowAndInputComponent.h"

#include <GLFW/glfw3.h>

 /**
  */
class WGLFWWindowAndInputComponent : public WWindowAndInputComponent {
public:
	WGLFWWindowAndInputComponent(class Wasabi* const app);

	virtual WError Initialize(int width, int height);
	virtual bool Loop();
	virtual void Cleanup();

	virtual void* GetPlatformHandle() const;
	virtual void* GetWindowHandle() const;
	virtual VkSurfaceKHR GetVulkanSurface() const;

	virtual void ShowErrorMessage(std::string error, bool warning = false);

	virtual void SetWindowTitle(const char* const title);
	virtual void SetWindowPosition(int x, int y);
	virtual void SetWindowSize(int width, int height);
	virtual void MaximizeWindow();
	virtual void MinimizeWindow();
	virtual uint RestoreWindow();

	virtual uint GetWindowWidth() const;
	virtual uint GetWindowHeight() const;
	virtual int GetWindowPositionX() const;
	virtual int GetWindowPositionY() const;

	virtual void SetFullScreenState(bool bFullScreen);
	virtual bool GetFullScreenState() const;
	virtual void SetWindowMinimumSize(int minX, int minY);
	virtual void SetWindowMaximumSize(int maxX, int maxY);

	virtual bool MouseClick(W_MOUSEBUTTON button) const;
	virtual int MouseX(W_MOUSEPOSTYPE posT = MOUSEPOS_VIEWPORT,
		uint vpID = 0) const;
	virtual int MouseY(W_MOUSEPOSTYPE posT = MOUSEPOS_VIEWPORT,
		uint vpID = 0) const;
	virtual int MouseZ() const;
	virtual bool MouseInScreen(W_MOUSEPOSTYPE posT = MOUSEPOS_VIEWPORT,
		uint vpID = 0) const;

	virtual void SetMousePosition(uint x, uint y, W_MOUSEPOSTYPE posT = MOUSEPOS_VIEWPORT);
	virtual void SetMouseZ(int value);
	virtual void ShowCursor(bool bShow);

	virtual void EnableEscapeKeyQuit();
	virtual void DisableEscapeKeyQuit();

	virtual bool KeyDown(unsigned int key) const;

	virtual void InsertRawInput(unsigned int key, bool state);

private:
	/** GLFW window */
	GLFWwindow* m_window;
	/** Primary monitor */
	GLFWmonitor* m_monitor;
	/** Vulkan surface */
	VkSurfaceKHR m_surface;
	/** Whether or not window is minimized */
	bool m_isMinimized;
	/** Whether or not to quit on escape */
	bool m_escapeQuit;
	/** true if mouse left button is clicked, false otherwise */
	bool m_leftClick;
	/** true if mouse right button is clicked, false otherwise */
	bool m_rightClick;
	/** true if mouse middle button is clicked, false otherwise */
	bool m_middleClick;
	/** Whether or not the mouse is in the window */
	bool m_isMouseInScreen;
	/** Scroll value for the mouse */
	int m_mouseZ;
	/** states of all keys */
	bool m_keyDown[350];
	/** Size limits for GLFW window */
	int m_windowSizeLimits[4];
	/** Stored x, y, width, height, before going fullscreen */
	int m_storedWindowDimensions[4];

	/** Registers GLFW callbacks */
	void SetCallbacks();
};