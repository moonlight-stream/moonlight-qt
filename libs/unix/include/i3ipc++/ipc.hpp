#pragma once

#include <cstdint>
#include <list>
#include <string>
#include <memory>
#include <vector>
#include <map>

#include <sigc++/sigc++.h>

extern "C" {
#include <i3/ipc.h>
}

/**
 * @addtogroup i3ipc i3 IPC C++ binding
 * @{
 */
namespace i3ipc {

/**
 * Get path to the i3 IPC socket
 * @return Path to a socket
 */
std::string  get_socketpath();

/**
 * Primitive of rectangle
 */
struct rect_t {
	uint32_t  x; ///< Position on X axis
	uint32_t  y; ///< Position on Y axis
	uint32_t  width; ///< Width of rectangle
	uint32_t  height; ///< Height of rectangle
};

/**
 * i3's workspace
 */
struct workspace_t {
	int  num; ///< Index of the worksapce
	std::string  name; ///< Name of the workspace
	bool  visible; ///< Is the workspace visible
	bool  focused; ///< Is the workspace is currently focused
	bool  urgent; ///< Is the workspace is urgent
	rect_t  rect; ///< A size of the workspace
	std::string  output; ///< An output of the workspace
};

/**
 * i3's output
 */
struct output_t {
	std::string  name; ///< Name of the output
	bool  active; ///< Is the output currently active
	bool  primary; ///< Is the output the primary output
	std::string  current_workspace; ///< Name of current workspace
	rect_t  rect; ///< Size of the output
};

/**
 * Version of i3
 */
struct version_t {
	std::string  human_readable; ///< Human redable version string
	std::string  loaded_config_file_name; ///< Path to current config of i3
	uint32_t  major; ///< Major version of i3
	uint32_t  minor; ///< Minor version of i3
	uint32_t  patch; ///< Patch number of i3
};


/**
 * Types of the events of i3
 */
enum EventType {
	ET_WORKSPACE = (1 << 0), ///< Workspace event
	ET_OUTPUT = (1 << 1), ///< Output event
	ET_MODE = (1 << 2), ///< Output mode event
	ET_WINDOW = (1 << 3), ///< Window event
	ET_BARCONFIG_UPDATE = (1 << 4), ///< Bar config update event @attention Yet is not implemented as signal in connection
	ET_BINDING = (1 << 5), ///< Binding event
};

/**
 * Types of workspace events
 */
enum class WorkspaceEventType : char {
	FOCUS = 'f', ///< Focused
	INIT = 'i', ///< Initialized
	EMPTY = 'e', ///< Became empty
	URGENT = 'u', ///< Became urgent
	RENAME = 'r', ///< Renamed
	RELOAD = 'l', ///< Reloaded
	RESTORED = 's', ///< Restored
};

/**
 * Types of window events
 */
enum class WindowEventType : char {
	NEW = 'n', ///< Window created
	CLOSE = 'c', ///< Window closed
	FOCUS = 'f', ///< Window got focus
	TITLE = 't', ///< Title of window has been changed
	FULLSCREEN_MODE = 'F', ///< Window toggled to fullscreen mode
	MOVE = 'M', ///< Window moved
	FLOATING = '_', ///< Window toggled floating mode
	URGENT = 'u', ///< Window became urgent
};


/**
 * A style of a container's border
 */
enum class BorderStyle : char {
	UNKNOWN = '?', //< If got an unknown border style in reply
	NONE = 'N',
	NORMAL = 'n',
	PIXEL = 'P',
	ONE_PIXEL = '1',
};


/**
 * A type of a container's layout
 */
enum class ContainerLayout : char {
	UNKNOWN = '?', //< If got an unknown border style in reply
	SPLIT_H = 'h',
	SPLIT_V = 'v',
	STACKED = 's',
	TABBED = 't',
	DOCKAREA = 'd',
	OUTPUT = 'o',
};


/**
 * A type of the input of bindings
 */
enum class InputType : char {
	UNKNOWN = '?', //< If got an unknown input_type in binding_event
	KEYBOARD = 'k',
	MOUSE = 'm',
};


/**
 * A mode of a bar
 */
enum class BarMode : char {
	UNKNOWN = '?',
	DOCK = 'd', ///< The bar sets the dock window type
	HIDE = 'h', ///< The bar does not show unless a specific key is pressed
};


/**
 * A position (of a bar?)
 */
enum class Position : char {
	UNKNOWN = '?',
	TOP = 't',
	BOTTOM = 'b',
};


/**
 * X11 window properties
 */
struct window_properties_t {
	std::string  xclass; /// X11 Window class (WM_CLASS class)
	std::string  instance; ///X11 Window class instance (WM_CLASS instance)
	std::string  window_role; /// X11 Window role (WM_WINDOW_ROLE)
	std::string  title; /// X11 UTF8 window title (_NET_WM_NAME)
	uint64_t  transient_for; /// Logical top-level window. Nonzero value is an X11 window ID of the parent window (WM_TRANSIENT_FOR)
};

/**
 * A node of tree of windows
 */
struct container_t {
	uint64_t  id; ///< The internal ID (actually a C pointer value) of this container. Do not make any assumptions about it. You can use it to (re-)identify and address containers when talking to i3
	uint64_t  xwindow_id; ///< The X11 window ID of the actual client window inside this container. This field is set to null for split containers or otherwise empty containers. This ID corresponds to what xwininfo(1) and other X11-related tools display (usually in hex)
	std::string  name; ///< The internal name of this container. For all containers which are part of the tree structure down to the workspace contents, this is set to a nice human-readable name of the container. For containers that have an X11 window, the content is the title (_NET_WM_NAME property) of that window. For all other containers, the content is not defined (yet)
	std::string  type; ///< Type of this container
	BorderStyle  border; ///< A style of the container's border
	std::string  border_raw; ///< A "border" field of TREE reply. NOT empty only if border equals BorderStyle::UNKNOWN
	uint32_t  current_border_width; ///< Number of pixels of the border width
	ContainerLayout  layout; ///< A type of the container's layout
	std::string  layout_raw; ///< A "layout" field of TREE reply. NOT empty only if layout equals ContainerLayout::UNKNOWN
	float  percent; ///< The percentage which this container takes in its parent. A value of < 0 means that the percent property does not make sense for this container, for example for the root container.
	rect_t  rect; ///< The absolute display coordinates for this container
	rect_t  window_rect; ///< The coordinates of the actual client window inside its container. These coordinates are relative to the container and do not include the window decoration (which is actually rendered on the parent container)
	rect_t  deco_rect; ///< The coordinates of the window decoration inside its container. These coordinates are relative to the container and do not include the actual client window
	rect_t  geometry; ///< The original geometry the window specified when i3 mapped it. Used when switching a window to floating mode, for example
	bool  urgent;
	bool  focused;

	window_properties_t  window_properties; /// X11 window properties

	std::list< std::shared_ptr<container_t> >  nodes;
};


/**
 * A workspace event
 */
struct workspace_event_t {
	WorkspaceEventType  type;
	std::shared_ptr<workspace_t>  current; ///< Current focused workspace
	std::shared_ptr<workspace_t>  old; ///< Old (previous) workspace @note With some WindowEventType could be null
};


/**
 * A window event
 */
struct window_event_t {
	WindowEventType  type;
	std::shared_ptr<container_t>  container; ///< A container event associated with @note With some WindowEventType could be null
};


/**
 * A binding
 */
struct binding_t {
	std::string  command; ///< The i3 command that is configured to run for this binding
	std::vector<std::string>  event_state_mask; ///< The group and modifier keys that were configured with this binding
	int32_t  input_code; ///< If the binding was configured with bindcode, this will be the key code that was given for the binding. If the binding is a mouse binding, it will be the number of the mouse button that was pressed. Otherwise it will be 0
	std::string  symbol; ///< If this is a keyboard binding that was configured with bindsym, this field will contain the given symbol. Otherwise it will be null
	InputType  input_type;
};


/**
 * A mode
 */
struct mode_t {
	std::string  change; ///< The current mode in use
	bool pango_markup; ///< Should pango markup be used for displaying this mode
};


/**
 * A bar configuration
 */
struct bar_config_t {
	std::string  id; ///< The ID for this bar. Included in case you request multiple configurations and want to differentiate the different replies.
	BarMode  mode;
	Position  position;
	std::string  status_command; ///< Command which will be run to generate a statusline. Each line on stdout of this command will be displayed in the bar. At the moment, no formatting is supported
	std::string  font; ///< The font to use for text on the bar
	bool  workspace_buttons; ///< Display workspace buttons or not? Defaults to true.
	bool  binding_mode_indicator; ///< Display the mode indicator or not? Defaults to true.
	bool  verbose; ///< Should the bar enable verbose output for debugging? Defaults to false.
	std::map<std::string, uint32_t>  colors; ///< Contains key/value pairs of colors. Each value is a color code in format 0xRRGGBB
};


struct buf_t;
/**
 * Connection to the i3
 */
class connection {
public:
	/**
	 * Connect to the i3
	 * @param  socket_path path to a i3 IPC socket
	 */
	connection(const std::string&  socket_path = get_socketpath());
	~connection();

	/**
	 * Send a command to i3
	 * @param  command command
	 * @return         Is command successfully executed
	 */
	bool  send_command(const std::string&  command) const;

	/**
	 * Request a list of workspaces
	 * @return List of workspaces
	 */
	std::vector< std::shared_ptr<workspace_t> >  get_workspaces() const;

	/**
	 * Request a list of outputs
	 * @return List of outputs
	 */
	std::vector< std::shared_ptr<output_t> >  get_outputs() const;

	/**
	 * Request a version of i3
	 * @return Version of i3
	 */
	version_t  get_version() const;

	/**
	 * Request a tree of windows
	 * @return A root container
	 */
	std::shared_ptr<container_t>  get_tree() const;

	/**
	 * Request a list of names of available barconfigs
	 * @return A list of names of barconfigs
	 */
	std::vector<std::string>  get_bar_configs_list() const;

	/**
	 * Request a barconfig
	 * @param  name  name of barconfig
	 * @return The barconfig
	 */
	std::shared_ptr<bar_config_t>  get_bar_config(const std::string&  name) const;

	/**
	 * Subscribe on an events of i3
	 * 
	 * If connection isn't handling events at the moment, event numer will be added to subscription list.
	 * Else will also send subscripe request to i3
	 *
	 * Example:
	 * @code{.cpp}
	 * connection  conn;
	 * conn.subscribe(i3ipc::ipc::ET_WORKSPACE | i3ipc::ipc::ET_WINDOW);
	 * @endcode
	 * 
	 * @param  events event type (EventType enum)
	 * @return        Is successfully subscribed. If connection isn't handling events at the moment, then always true.
	 */
	bool  subscribe(const int32_t  events);

	/**
	 * Handle an event from i3
	 * @note Used only in main()
	 */
	void  handle_event();

	/**
	 * Get the fd of the main socket
	 * @return the file descriptor of the main socket.
	 */
	int32_t get_main_socket_fd();

	/**
	 * Get the fd of the event socket
	 * @return the file descriptor of the event socket.
	 */
	int32_t get_event_socket_fd();

	/**
	 * Connect the event socket to IPC
	 * @param  reconnect if true the event socket will be disconnected and connected again
	 * @note Automaticly called, when calling handle_event();
	 */
	void  connect_event_socket(const bool  reconnect = false);

	/**
	 * Disconnect the event socket
	 */
	void  disconnect_event_socket();

	sigc::signal<void, const workspace_event_t&>  signal_workspace_event; ///< Workspace event signal
	sigc::signal<void>  signal_output_event; ///< Output event signal
	sigc::signal<void, const mode_t&>  signal_mode_event; ///< Output mode event signal
	sigc::signal<void, const window_event_t&>  signal_window_event; ///< Window event signal
	sigc::signal<void, const bar_config_t&>  signal_barconfig_update_event; ///< Barconfig update event signal
	sigc::signal<void, const binding_t&>  signal_binding_event; ///< Binding event signal
	sigc::signal<void, EventType, const std::shared_ptr<const buf_t>&>  signal_event; ///< i3 event signal @note Default handler routes event to signal according to type
private:
	const int32_t  m_main_socket;
	int32_t  m_event_socket;
	int32_t  m_subscriptions;
	const std::string  m_socket_path;
};

/**
 * Get version of i3ipc++
 * @return the version of i3ipc++
 */
const version_t&  get_version();

}

/**
 * @}
 */
