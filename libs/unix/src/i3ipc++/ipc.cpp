#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <iostream>

#include <auss.hpp>
#include <json/json.h>

#include <i3ipc++/log.hpp>
#include <i3ipc++/ipc-util.hpp>
#include <i3ipc++/ipc.hpp>

namespace i3ipc {

// For log.hpp
std::vector<std::ostream*>  g_logging_outs = {
	&std::cout,
};
std::vector<std::ostream*>  g_logging_err_outs = {
	&std::cerr,
};

#define IPC_JSON_READ(ROOT) \
	{ \
		Json::Reader  reader; \
		if (!reader.parse(std::string(buf->payload, buf->header->size), ROOT, false)) { \
			throw invalid_reply_payload_error(auss_t() << "Failed to parse reply on \"" i3IPC_TYPE_STR "\": " << reader.getFormattedErrorMessages()); \
		} \
	}

#define IPC_JSON_ASSERT_TYPE(OBJ, OBJ_DESCR, TYPE_CHECK, TYPE_NAME) \
	{\
		if (!(OBJ).TYPE_CHECK()) { \
			throw invalid_reply_payload_error(auss_t() << "Failed to parse reply on \"" i3IPC_TYPE_STR "\": " OBJ_DESCR " expected to be " TYPE_NAME); \
		} \
	}
#define IPC_JSON_ASSERT_TYPE_OBJECT(OBJ, OBJ_DESCR) IPC_JSON_ASSERT_TYPE(OBJ, OBJ_DESCR, isObject, "an object")
#define IPC_JSON_ASSERT_TYPE_ARRAY(OBJ, OBJ_DESCR) IPC_JSON_ASSERT_TYPE(OBJ, OBJ_DESCR, isArray, "an array")
#define IPC_JSON_ASSERT_TYPE_BOOL(OBJ, OBJ_DESCR) IPC_JSON_ASSERT_TYPE(OBJ, OBJ_DESCR, isBool, "a bool")
#define IPC_JSON_ASSERT_TYPE_INT(OBJ, OBJ_DESCR) IPC_JSON_ASSERT_TYPE(OBJ, OBJ_DESCR, isInt, "an integer")


inline rect_t  parse_rect_from_json(const Json::Value&  value) {
	return {
		.x = value["x"].asUInt(),
		.y = value["y"].asUInt(),
		.width = value["width"].asUInt(),
		.height = value["height"].asUInt(),
	};
}

window_properties_t  parse_window_props_from_json(const Json::Value&  value) {
	if (value.isNull()) {
		window_properties_t result;
		result.transient_for = 0ull;
		return result;
	}

	window_properties_t result {
		value["class"].asString(),
		value["instance"].asString(),
		value["window_role"].asString(),
		value["title"].asString(),
		0ull
	};

	const Json::Value transient_for = value["transient_for"];
	if (!transient_for.isNull()) {
		result.transient_for = transient_for.asUInt64();
	}

	return result;
}


static std::shared_ptr<container_t>  parse_container_from_json(const Json::Value&  o) {
#define i3IPC_TYPE_STR "PARSE CONTAINER FROM JSON"
	if (o.isNull())
		return std::shared_ptr<container_t>();
	std::shared_ptr<container_t>  container (new container_t());
	IPC_JSON_ASSERT_TYPE_OBJECT(o, "o")

	container->id = o["id"].asUInt64();
	container->xwindow_id= o["window"].asUInt64();
	container->name = o["name"].asString();
	container->type = o["type"].asString();
	container->current_border_width = o["current_border_width"].asInt();
	container->percent = o["percent"].asFloat();
	container->rect = parse_rect_from_json(o["rect"]);
	container->window_rect = parse_rect_from_json(o["window_rect"]);
	container->deco_rect = parse_rect_from_json(o["deco_rect"]);
	container->geometry = parse_rect_from_json(o["geometry"]);
	container->urgent = o["urgent"].asBool();
	container->focused = o["focused"].asBool();

	container->border = BorderStyle::UNKNOWN;
	std::string  border = o["border"].asString();
	if (border == "normal") {
		container->border = BorderStyle::NORMAL;
	} else if (border == "none") {
		container->border = BorderStyle::NONE;
	} else if (border == "pixel") {
		container->border = BorderStyle::PIXEL;
	} else if (border == "1pixel") {
		container->border = BorderStyle::ONE_PIXEL;
	} else {
		container->border_raw = border;
		I3IPC_WARN("Got a unknown \"border\" property: \"" << border << "\". Perhaps its neccessary to update i3ipc++. If you are using latest, note maintainer about this")
	}

	container->layout = ContainerLayout::UNKNOWN;
	std::string  layout = o["layout"].asString();

	if (layout == "splith") {
		container->layout = ContainerLayout::SPLIT_H;
	} else if (layout == "splitv") {
		container->layout = ContainerLayout::SPLIT_V;
	} else if (layout == "stacked") {
		container->layout = ContainerLayout::STACKED;
	} else if (layout == "tabbed") {
		container->layout = ContainerLayout::TABBED;
	} else if (layout == "dockarea") {
		container->layout = ContainerLayout::DOCKAREA;
	} else if (layout == "output") {
		container->layout = ContainerLayout::OUTPUT;
	} else {
		container->layout_raw = border;
		I3IPC_WARN("Got a unknown \"layout\" property: \"" << layout << "\". Perhaps its neccessary to update i3ipc++. If you are using latest, note maintainer about this")
	}

	Json::Value  nodes = o["nodes"];
	if (!nodes.isNull()) {
		IPC_JSON_ASSERT_TYPE_ARRAY(nodes, "nodes")
		for (Json::ArrayIndex  i = 0; i < nodes.size(); i++) {
			container->nodes.push_back(parse_container_from_json(nodes[i]));
		}
	}

	container->window_properties = parse_window_props_from_json(o["window_properties"]);

	return container;
#undef i3IPC_TYPE_STR
}

static std::shared_ptr<workspace_t>  parse_workspace_from_json(const Json::Value&  value) {
	if (value.isNull())
		return std::shared_ptr<workspace_t>();
	Json::Value  num = value["num"];
	Json::Value  name = value["name"];
	Json::Value  visible = value["visible"];
	Json::Value  focused = value["focused"];
	Json::Value  urgent = value["urgent"];
	Json::Value  rect = value["rect"];
	Json::Value  output = value["output"];

	std::shared_ptr<workspace_t>  p (new workspace_t());
	p->num = num.asInt();
	p->name = name.asString();
	p->visible = visible.asBool();
	p->focused = focused.asBool();
	p->urgent = urgent.asBool();
	p->rect = parse_rect_from_json(rect);
	p->output = output.asString();
	return p;
}

static std::shared_ptr<output_t>  parse_output_from_json(const Json::Value&  value) {
	if (value.isNull())
		return std::shared_ptr<output_t>();
	Json::Value  name = value["name"];
	Json::Value  active = value["active"];
	Json::Value  primary = value["primary"];
	Json::Value  current_workspace = value["current_workspace"];
	Json::Value  rect = value["rect"];

	std::shared_ptr<output_t>  p (new output_t());
	p->name = name.asString();
	p->active = active.asBool();
	p->primary = primary.asBool();
	p->current_workspace = (current_workspace.isNull() ? std::string() : current_workspace.asString());
	p->rect = parse_rect_from_json(rect);
	return p;
}

static std::shared_ptr<binding_t>  parse_binding_from_json(const Json::Value&  value) {
#define i3IPC_TYPE_STR "PARSE BINDING FROM JSON"
	if (value.isNull())
		return std::shared_ptr<binding_t>();
	IPC_JSON_ASSERT_TYPE_OBJECT(value, "binding")
	std::shared_ptr<binding_t>  b (new binding_t());

	b->command = value["command"].asString();
	b->symbol = value["symbol"].asString();
	b->input_code = value["input_code"].asInt();

	Json::Value input_type = value["input_type"].asString();
	if (input_type == "keyboard") {
		b->input_type = InputType::KEYBOARD;
	} else if (input_type == "mouse") {
		b->input_type = InputType::MOUSE;
	} else {
		b->input_type = InputType::UNKNOWN;
	}

	Json::Value  esm_arr = value["event_state_mask"];
	IPC_JSON_ASSERT_TYPE_ARRAY(esm_arr, "event_state_mask")

	b->event_state_mask.resize(esm_arr.size());

	for (Json::ArrayIndex  i = 0; i < esm_arr.size(); i++) {
		b->event_state_mask[i] = esm_arr[i].asString();
	}

	return b;
#undef i3IPC_TYPE_STR
}

static std::shared_ptr<mode_t>  parse_mode_from_json(const Json::Value&  value) {
	if (value.isNull())
		return std::shared_ptr<mode_t>();
	Json::Value  change = value["change"];
	Json::Value  pango_markup = value["pango_markup"];

	std::shared_ptr<mode_t>  p (new mode_t());
	p->change = change.asString();
	p->pango_markup = pango_markup.asBool();
	return p;
}


static std::shared_ptr<bar_config_t>  parse_bar_config_from_json(const Json::Value&  value) {
#define i3IPC_TYPE_STR "PARSE BAR CONFIG FROM JSON"
	if (value.isNull())
		return std::shared_ptr<bar_config_t>();
	IPC_JSON_ASSERT_TYPE_OBJECT(value, "(root)")
	std::shared_ptr<bar_config_t>  bc (new bar_config_t());

	bc->id = value["id"].asString();
	bc->status_command = value["status_command"].asString();
	bc->font = value["font"].asString();
	bc->workspace_buttons = value["workspace_buttons"].asBool();
	bc->binding_mode_indicator = value["binding_mode_indicator"].asBool();
	bc->verbose = value["verbose"].asBool();

	std::string  mode = value["mode"].asString();
	if (mode == "dock") {
		bc->mode = BarMode::DOCK;
	} else if (mode == "hide") {
		bc->mode = BarMode::HIDE;
	} else {
		bc->mode = BarMode::UNKNOWN;
		I3IPC_WARN("Got a unknown \"mode\" property: \"" << mode << "\". Perhaps its neccessary to update i3ipc++. If you are using latest, note maintainer about this")
	}

	std::string  position = value["position"].asString();
	if (position == "top") {
		bc->position = Position::TOP;
	} else if (position == "bottom") {
		bc->position = Position::BOTTOM;
	} else {
		bc->position = Position::UNKNOWN;
		I3IPC_WARN("Got a unknown \"position\" property: \"" << position << "\". Perhaps its neccessary to update i3ipc++. If you are using latest, note maintainer about this")
	}

	Json::Value  colors = value["colors"];
	IPC_JSON_ASSERT_TYPE_OBJECT(value, "colors")
	auto  colors_list = colors.getMemberNames();
	for (auto&  m : colors_list) {
		bc->colors[m] = std::stoul(colors[m].asString().substr(1), nullptr, 16);
	}

	return bc;
#undef i3IPC_TYPE_STR
}


std::string  get_socketpath() {
	std::string  str;
	do {
		const char *env = std::getenv("I3SOCK");
		if (!env) {
			break;
		}
		str = env;
		if (str.back() == '\n') {
			str.pop_back();
		}
		return str;
	} while(0);
	do {
		const char *env = std::getenv("SWAYSOCK");
		if (!env) {
			break;
		}
		str = env;
		if (str.back() == '\n') {
			str.pop_back();
		}
		return str;
	} while(0);
	do {
		auss_t  str_buf;
		FILE*  in;
		char  buf[512] = {0};
		if (!(in = popen("i3 --get-socketpath", "r"))) {
			break;
		}

		while (fgets(buf, sizeof(buf), in) != nullptr) {
			str_buf << buf;
		}
		pclose(in);
		str = str_buf;

		if (str.back() == '\n') {
			str.pop_back();
		}
		return str;
	} while (0);
	do {
		auss_t  str_buf;
		FILE*  in;
		char  buf[512] = {0};
		if (!(in = popen("sway --get-socketpath", "r"))) {
			break;
		}

		while (fgets(buf, sizeof(buf), in) != nullptr) {
			str_buf << buf;
		}
		pclose(in);
		str = str_buf;

		if (str.back() == '\n') {
			str.pop_back();
		}
		return str;
	} while (0);
	throw errno_error("Failed to get socket path");
}


connection::connection(const std::string&  socket_path) : m_main_socket(i3_connect(socket_path)), m_event_socket(-1), m_subscriptions(0), m_socket_path(socket_path) {
#define i3IPC_TYPE_STR "i3's event"
	signal_event.connect([this](EventType  event_type, const std::shared_ptr<const buf_t>&  buf) {
		switch (event_type) {
		case ET_WORKSPACE: {
			workspace_event_t  ev;
			Json::Value  root;
			IPC_JSON_READ(root);
			std::string  change = root["change"].asString();
			if (change == "focus") {
				ev.type = WorkspaceEventType::FOCUS;
			} else if (change == "init") {
				ev.type = WorkspaceEventType::INIT;
			} else if (change == "empty") {
				ev.type = WorkspaceEventType::EMPTY;
			} else if (change == "urgent") {
				ev.type = WorkspaceEventType::URGENT;
			} else if (change == "rename") {
				ev.type = WorkspaceEventType::RENAME;
			} else if (change == "reload") {
				ev.type = WorkspaceEventType::RELOAD;
			} else if (change == "restored") {
				ev.type = WorkspaceEventType::RESTORED;
			} else {
				I3IPC_WARN("Unknown workspace event type " << change)
				break;
			}
			I3IPC_DEBUG("WORKSPACE " << change)

			Json::Value  current = root["current"];
			Json::Value  old = root["old"];

			if (!current.isNull()) {
				ev.current = parse_workspace_from_json(current);
			}
			if (!old.isNull()) {
				ev.old = parse_workspace_from_json(old);
			}

			signal_workspace_event.emit(ev);
			break;
		}
		case ET_OUTPUT:
			I3IPC_DEBUG("OUTPUT")
			signal_output_event.emit();
			break;
		case ET_MODE: {
			I3IPC_DEBUG("MODE")
			Json::Value  root;
			IPC_JSON_READ(root);
			std::shared_ptr<mode_t>  mode_data = parse_mode_from_json(root);
			signal_mode_event.emit(*mode_data);
			break;
		}
		case ET_WINDOW: {
			window_event_t  ev;
			Json::Value  root;
			IPC_JSON_READ(root);
			std::string  change = root["change"].asString();
			if (change == "new") {
				ev.type = WindowEventType::NEW;
			} else if (change == "close") {
				ev.type = WindowEventType::CLOSE;
			} else if (change == "focus") {
				ev.type = WindowEventType::FOCUS;
			} else if (change == "title") {
				ev.type = WindowEventType::TITLE;
			} else if (change == "fullscreen_mode") {
				ev.type = WindowEventType::FULLSCREEN_MODE;
			} else if (change == "move") {
				ev.type = WindowEventType::MOVE;
			} else if (change == "floating") {
				ev.type = WindowEventType::FLOATING;
			} else if (change == "urgent") {
				ev.type = WindowEventType::URGENT;
			}
			I3IPC_DEBUG("WINDOW " << change)

			Json::Value  container = root["container"];
			if (!container.isNull()) {
				ev.container = parse_container_from_json(container);
			}

			signal_window_event.emit(ev);
			break;
		}
		case ET_BARCONFIG_UPDATE: {
			I3IPC_DEBUG("BARCONFIG_UPDATE")
			Json::Value  root;
			IPC_JSON_READ(root);
			std::shared_ptr<bar_config_t>  barconf = parse_bar_config_from_json(root);
			signal_barconfig_update_event.emit(*barconf);
			break;
		}
		case ET_BINDING: {
			Json::Value  root;
			IPC_JSON_READ(root);
			std::string  change = root["change"].asString();
			if (change != "run") {
				I3IPC_WARN("Got \"" << change << "\" in field \"change\" of binding_event. Expected \"run\"")
			}

			Json::Value  binding_json = root["binding"];
			std::shared_ptr<binding_t>  bptr;
			if (!binding_json.isNull()) {
				bptr = parse_binding_from_json(binding_json);
			}

			if (!bptr) {
				I3IPC_ERR("Failed to parse field \"binding\" from binding_event")
			} else {
				I3IPC_DEBUG("BINDING " << bptr->symbol);
				signal_binding_event.emit(*bptr);
			}
			break;
		}
		};
	});
#undef i3IPC_TYPE_STR
}
connection::~connection() {
	i3_disconnect(m_main_socket);
	if (m_event_socket > 0)
		this->disconnect_event_socket();
}


void  connection::connect_event_socket(const bool  reconnect) {
	if (m_event_socket > 0) {
		if (reconnect) {
			this->disconnect_event_socket();
		} else {
			I3IPC_ERR("Trying to initialize event socket secondary")
			return;
		}
	}
	m_event_socket = i3_connect(m_socket_path);
	this->subscribe(m_subscriptions);
}


void  connection::disconnect_event_socket() {
	if (m_event_socket <= 0) {
		I3IPC_WARN("Trying to disconnect non-connected event socket")
		return;
	}
	i3_disconnect(m_event_socket);
}


void  connection::handle_event() {
	if (m_event_socket <= 0) {
		this->connect_event_socket();
	}
	auto  buf = i3_recv(m_event_socket);

	this->signal_event.emit(static_cast<EventType>(1 << (buf->header->type & 0x7f)), std::static_pointer_cast<const buf_t>(buf));
}


bool  connection::subscribe(const int32_t  events) {
#define i3IPC_TYPE_STR "SUBSCRIBE"
	if (m_event_socket <= 0) {
		m_subscriptions |= events;
		return true;
	}
	std::string  payload;
	{
		auss_t  payload_auss;
		if (events & static_cast<int32_t>(ET_WORKSPACE)) {
			payload_auss << "\"workspace\",";
		}
		if (events & static_cast<int32_t>(ET_OUTPUT)) {
			payload_auss << "\"output\",";
		}
		if (events & static_cast<int32_t>(ET_MODE)) {
			payload_auss << "\"mode\",";
		}
		if (events & static_cast<int32_t>(ET_WINDOW)) {
			payload_auss << "\"window\",";
		}
		if (events & static_cast<int32_t>(ET_BARCONFIG_UPDATE)) {
			payload_auss << "\"barconfig_update\",";
		}
		if (events & static_cast<int32_t>(ET_BINDING)) {
			payload_auss << "\"binding\",";
		}
		payload = payload_auss;
		if (payload.empty()) {
			return true;
		}
		payload.pop_back();
	}
	I3IPC_DEBUG("i3 IPC subscriptions: " << payload)

	auto  buf = i3_msg(m_event_socket, ClientMessageType::SUBSCRIBE, auss_t() << '[' << payload << ']');
	Json::Value  root;
	IPC_JSON_READ(root)

	m_subscriptions |= events;

	return root["success"].asBool();
#undef i3IPC_TYPE_STR
}


version_t  connection::get_version() const {
#define i3IPC_TYPE_STR "GET_VERSION"
	auto  buf = i3_msg(m_main_socket, ClientMessageType::GET_VERSION);
	Json::Value  root;
	IPC_JSON_READ(root)
	IPC_JSON_ASSERT_TYPE_OBJECT(root, "root")

	return {
		.human_readable = root["human_readable"].asString(),
		.loaded_config_file_name = root["loaded_config_file_name"].asString(),
		.major = root["major"].asUInt(),
		.minor = root["minor"].asUInt(),
		.patch = root["patch"].asUInt(),
	};
#undef i3IPC_TYPE_STR
}


std::shared_ptr<container_t>  connection::get_tree() const {
#define i3IPC_TYPE_STR "GET_TREE"
	auto  buf = i3_msg(m_main_socket, ClientMessageType::GET_TREE);
	Json::Value  root;
	IPC_JSON_READ(root);
	return parse_container_from_json(root);
#undef i3IPC_TYPE_STR
}


std::vector< std::shared_ptr<output_t> >  connection::get_outputs() const {
#define i3IPC_TYPE_STR "GET_OUTPUTS"
	auto  buf = i3_msg(m_main_socket, ClientMessageType::GET_OUTPUTS);
	Json::Value  root;
	IPC_JSON_READ(root)
	IPC_JSON_ASSERT_TYPE_ARRAY(root, "root")

	std::vector< std::shared_ptr<output_t> >  outputs;

	for (auto w : root) {
		outputs.push_back(parse_output_from_json(w));
	}

	return outputs;
#undef i3IPC_TYPE_STR
}


std::vector< std::shared_ptr<workspace_t> >  connection::get_workspaces() const {
#define i3IPC_TYPE_STR "GET_WORKSPACES"
	auto  buf = i3_msg(m_main_socket, ClientMessageType::GET_WORKSPACES);
	Json::Value  root;
	IPC_JSON_READ(root)
	IPC_JSON_ASSERT_TYPE_ARRAY(root, "root")

	std::vector< std::shared_ptr<workspace_t> >  workspaces;

	for (auto w : root) {
		workspaces.push_back(parse_workspace_from_json(w));
	}

	return workspaces;
#undef i3IPC_TYPE_STR
}


std::vector<std::string>  connection::get_bar_configs_list() const {
#define i3IPC_TYPE_STR "GET_BAR_CONFIG (get_bar_configs_list)"
	auto  buf = i3_msg(m_main_socket, ClientMessageType::GET_BAR_CONFIG);
	Json::Value  root;
	IPC_JSON_READ(root)
	IPC_JSON_ASSERT_TYPE_ARRAY(root, "root")

	std::vector<std::string>  l;

	for (auto w : root) {
		l.push_back(w.asString());
	}

	return l;
#undef i3IPC_TYPE_STR
}


std::shared_ptr<bar_config_t>  connection::get_bar_config(const std::string&  name) const {
#define i3IPC_TYPE_STR "GET_BAR_CONFIG"
	auto  buf = i3_msg(m_main_socket, ClientMessageType::GET_BAR_CONFIG, name);
	Json::Value  root;
	IPC_JSON_READ(root)
	return parse_bar_config_from_json(root);
#undef i3IPC_TYPE_STR
}


bool  connection::send_command(const std::string&  command) const {
#define i3IPC_TYPE_STR "COMMAND"
	auto  buf = i3_msg(m_main_socket, ClientMessageType::COMMAND, command);
	Json::Value  root;
	IPC_JSON_READ(root)
	IPC_JSON_ASSERT_TYPE_ARRAY(root, "root")
	Json::Value  payload = root[0];
	IPC_JSON_ASSERT_TYPE_OBJECT(payload, " first item of root")

	if (payload["success"].asBool()) {
		return true;
	} else {
		Json::Value  error = payload["error"];
		if (!error.isNull()) {
			I3IPC_ERR("Failed to execute command: " << error.asString())
		}
		return false;
	}
#undef i3IPC_TYPE_STR
}

int32_t  connection::get_main_socket_fd() { return m_main_socket; }

int32_t  connection::get_event_socket_fd() { return m_event_socket; }


const version_t&  get_version() {
#define I3IPC_VERSION_MAJOR  0
#define I3IPC_VERSION_MINOR  4
#define I3IPC_VERSION_PATCH  0
	static version_t  version = {
		.human_readable = auss_t() << I3IPC_VERSION_MAJOR << '.' << I3IPC_VERSION_MINOR << '.' << I3IPC_VERSION_PATCH,
		.loaded_config_file_name = std::string(),
		.major = I3IPC_VERSION_MAJOR,
		.minor = I3IPC_VERSION_MINOR,
		.patch = I3IPC_VERSION_PATCH,
	};
	return version;
}

}
