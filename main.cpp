#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <list>
#include <ctype.h>
#include <map>
#include <sstream>
#include <assert.h>

struct evl_token
{
	enum token_type{ NAME, NUMBER, SINGLE };
	token_type type;

	std::string str;
	int line_no;
};
typedef std::list<evl_token> evl_tokens;

struct evl_statement
{
	enum statement_type { MODULE, WIRE, COMPONENT, ENDMODULE };
	statement_type type;

	evl_tokens tokens;
};
typedef std::list<evl_statement> evl_statements;

struct evl_wire
{
	std::string name;
	int width;
};
typedef std::list<evl_wire> evl_wires;

struct evl_pin
{
	std::string name;
	int pin_msb;
	int pin_lsb;
};
typedef std::list<evl_pin> evl_pins;

struct evl_component
{
	std::string name;
	std::string type;
	evl_pins pins;
};
typedef std::list<evl_component> evl_components;

typedef std::map<std::string, int> evl_wires_table;

class netlist;
class gate;
class net;
class pin;

class net
{
	std::list<pin *> connections_;
public:
	std::string net_name;
	net() {}
	void append_pin(pin *p);
	void display(std::ostream &out);
};

class pin
{
	gate *gate_;
	size_t pin_index_;
	std::vector<net *> nets_;
public:
	pin() {}				
	bool create(gate *g, size_t pin_index,
		const evl_pin &p,
		const std::map<std::string, net *> &nets_table,
		const evl_wires_table &wires_table);
	void display_for_component(std::ostream &out);
	void display_for_net(std::ostream &out);
}; 

class gate
{
	std::string type;
	std::string name;
	std::vector<pin*> pins_;
public:
	gate() {}		
	bool create_pin(const evl_pin &ep,
		size_t pin_index,
		const std::map<std::string, net *> &nets_table,
		const evl_wires_table &wires_table);
	bool create(const evl_component &c,
		const std::map<std::string, net *> &nets_table,
		const evl_wires_table &wires_table);
	void display(std::ostream &out);
	void dispay_for_type_name(std::ostream &out);
}; 

class netlist
{
	std::list<gate*> gates_;
	std::list<net*> nets_;
	std::map<std::string, net *> nets_table_;
public:
	netlist() {}	
	bool create_gate(const evl_component &c,
		const evl_wires_table &wires_table);
	bool create_gates(const evl_components &com,
		const evl_wires_table &wires_table);
	void create_net(std::string net_name);
	bool create_nets(const evl_wires &wires);
	bool create(const evl_wires &wires,
		const evl_components &comps,
		const evl_wires_table &wires_table);
	void display(std::ostream &out, evl_statements &statements);
};

bool process_wire_statement(evl_wires &wires, evl_statement &sm)    
{
	enum state_type {
		INIT, WIRE, DONE,
		WIRE_NAME, BUS_MSB, BUS_DONE,
		BUS_COLON, BUS_LSB
	};
	int wire_width = 1;
	state_type state = INIT;
	for (;
		!sm.tokens.empty() && (state != DONE);
		sm.tokens.pop_front())
	{
		evl_token co = sm.tokens.front();
		if (state == INIT)
		{
			if (co.str == "wire")
			{
				state = WIRE;
			}
			else{
				std::cerr << "Need 'wire' but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == WIRE)
		{
			if (co.type == evl_token::NAME)
			{
				evl_wire wire;

				wire.name = co.str;

				wire.width = wire_width;
				wires.push_back(wire);
				state = WIRE_NAME;
			}
			else if (co.str == "[")
				state = BUS_MSB;
			else{
				std::cerr << "Need NAME or '[' but founde '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == BUS_MSB)
		{
			if (co.type == evl_token::NUMBER)
			{
				wire_width = atoi(co.str.c_str()) + 1;
				state = BUS_COLON;
			}
			else
			{
				std::cerr << "Need NNUMBER but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == BUS_COLON)
		{
			if (co.str == ":")
				state = BUS_LSB;
			else{
				std::cerr << "Need ':' but founde '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == BUS_LSB)
		{
			if (co.type == evl_token::NUMBER)
			{
				wire_width = wire_width - atoi(co.str.c_str());
				state = BUS_DONE;
			}
			else
			{
				std::cerr << "Need NUMBER after ':' but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == BUS_DONE)
		{
			if (co.str == "]")
				state = WIRE;
		}
		else if (state == WIRE_NAME)
		{
			if (co.str == ";")
			{
				state = DONE;
			}
			else if (co.str == "]")
				state = WIRE_NAME;
			else if (co.str == ",")
			{
				state = WIRE;
			}
			else
			{
				std::cerr << "Need ', ; ]' but found '" << co.str << "' on line "
					<< co.line_no << std::endl;
				return false;
			}
		}
	}
	if (!sm.tokens.empty() || state != DONE)
	{
		std::cerr << "statement error" << std::endl;
		return false;
	}
	return true;
}

bool process_component_statement(evl_components &components, evl_statement &sm)		
{
	enum state_type {
		TYPE, NAME, PINS,
		DONE, COMP_NAME, PINS_DONE,
		PIN_MSB, PIN_LSB, PIN_COLON
	};
	state_type state = TYPE;
	evl_component component;
	evl_pin pin;
	pin.pin_msb = -1;
	pin.pin_lsb = -1;
	for (; !sm.tokens.empty() && (state != DONE); sm.tokens.pop_front())
	{
		evl_token co = sm.tokens.front();
		if (state == TYPE)
		{
			if (co.type == evl_token::NAME)
			{
				state = NAME;
				component.type = co.str;
			}
			else
			{
				std::cerr << "Need NAME but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == NAME)
		{
			if (co.type == evl_token::NAME)
			{
				component.name = co.str;
				state = COMP_NAME;
			}
			else
			{
				if (co.str == "(")
					state = PINS;
				else
				{
					std::cerr << "Need '(' but found '" << co.str
						<< "' on line " << co.line_no << std::endl;
					return false;
				}
			}
		}
		else if (state == PINS)
		{
			if (co.type == evl_token::NAME)
			{
				pin.name = co.str;
				state = COMP_NAME;
			}
			else
			{
				std::cerr << "Need Name but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == PIN_MSB)
		{
			if (co.type == evl_token::NUMBER)
			{
				pin.pin_msb = atoi(co.str.c_str());
				state = PIN_COLON;
			}
			else
			{
				std::cerr << "Need NUMBER of BUS MSB but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == PIN_COLON)
		{
			if (co.str == ":")
			{
				state = PIN_LSB;
			}
			else if (co.str == "]")
			{
				state = COMP_NAME;
			}
			else
			{
				std::cerr << "Need ':' or ']' but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == PIN_LSB)
		{
			if (co.type == evl_token::NUMBER)
			{
				pin.pin_lsb = atoi(co.str.c_str());
				state = COMP_NAME;
			}
			else
			{
				std::cerr << "Need NUMBER of BUS LSB but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == COMP_NAME)
		{
			if (co.str == "(")
			{
				state = PINS;
			}
			else if (co.str == "[")
			{
				state = PIN_MSB;
			}
			else if (co.str == "]")
			{
				state = COMP_NAME;
			}

			else if (co.str == ",")
			{
				component.pins.push_back(pin);
				pin.pin_msb = -1;
				pin.pin_lsb = -1;
				state = PINS;
			}
			else if (co.str == ")")
			{
				state = PINS_DONE;
				component.pins.push_back(pin);
			}
			else{
				std::cerr << "Need '( ) 'or ' [ ]' or ',' but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
		else if (state == PINS_DONE)
		{
			if (co.str == ";")
			{
				state = DONE;
			}
			else{
				std::cerr << "Need ';' but found '" << co.str
					<< "' on line " << co.line_no << std::endl;
				return false;
			}
		}
	}
	if (!sm.tokens.empty() || (state != DONE))
	{
		std::cerr << "statement error" << std::endl;
		return false;
	}
	else
	{
		components.push_back(component);
	}
	return true;
}

bool extract_tokens_from_line(std::string line, int line_no,
	evl_tokens &tokens)
{
	for (size_t i = 0; i<line.size();)
	{
		if (line[i] == '/')
		{
			++i;
			if ((i == line.size() || line[i] != '/')){
				std::cerr << "LINE " << line_no << ": a single / is error" << std::endl;
				return false;
			}
			break;
		}
		else if ((line[i] == ' ') || (line[i] == '\t')
			|| (line[i] == '\r') || (line[i] == '\n'))
		{
			++i;
			continue;
		}
		else if ((line[i] == '(') || (line[i] == ')')
			|| (line[i] == '[') || (line[i] == ']')
			|| (line[i] == ':') || (line[i] == ';')
			|| (line[i] == ','))
		{
			evl_token token;
			token.line_no = line_no;
			token.type = evl_token::SINGLE;
			token.str = std::string(1, line[i]);
			tokens.push_back(token);
			++i;
			continue;
		}
		else if (isalpha(line[i]) || (line[i] == '_'))
		{
			size_t name_begin = i;
			for (++i; i < line.size(); ++i)
			{
				if (!(isalpha(line[i]) || (line[i] == '_')
					|| (line[i]) == '$' || isdigit(line[i])))
				{
					break;
				}
			}
			evl_token token;
			token.line_no = line_no;
			token.type = evl_token::NAME;
			token.str = line.substr(name_begin, i - name_begin);
			tokens.push_back(token);
			continue;
		}
		else if (isdigit(line[i]))
		{
			size_t num_begin = i;
			while (i < line.size())
			{
				++i;
				if (!isdigit(line[i]))
				{
					break;
				}
			}
			evl_token token;
			token.line_no = line_no;
			token.type = evl_token::NUMBER;
			token.str = line.substr(num_begin, i - num_begin);
			tokens.push_back(token);
			continue;
		}
		else
		{
			std::cerr << "LINE" << line_no << "error character"
				<< std::endl;
			return false;
		}
	}
	return true;
}

bool extract_tokens_from_file(std::string file_name,
	evl_tokens &tokens)
{
	std::ifstream input_file(file_name.c_str());
	if (!input_file)
	{
		std::cerr << "I cant't read " << file_name << "." << std::endl;
		return 0;
	}
	tokens.clear();
	std::string line;
	for (int line_no = 1; std::getline(input_file, line); ++line_no)
	{
		if (!extract_tokens_from_line(line, line_no, tokens))
		{
			return false;
		}

	}
	return true;
}

bool group_tokens_into_statements(evl_statements &statements,
	evl_tokens &tokens)
{
	while (!tokens.empty())
	{
		evl_token token = tokens.front();
		if (token.type != evl_token::NAME)
		{
			std::cerr << "Need a NAME token but found '" << token.str
				<< "' on line " << token.line_no << std::endl;
			return false;
		}
		if (token.str == "module")
		{
			evl_statement module;
			module.type = evl_statement::MODULE;
			while (!tokens.empty())
			{
				module.tokens.push_back(tokens.front());
				tokens.pop_front();
				if (module.tokens.back().str == ";")
					break;
			}
			if (module.tokens.back().str != ";")
			{
				std::cerr << "Look for ';' but reach the end" << std::endl;
				return false;
			}
			statements.push_back(module);
		}
		else if (token.str == "endmodule")
		{
			evl_statement endmodule;
			endmodule.type = evl_statement::ENDMODULE;
			endmodule.tokens.push_back(tokens.front());
			tokens.pop_front();
			statements.push_back(endmodule);
		}
		else if (token.str == "wire")
		{
			evl_statement wire;
			wire.type = evl_statement::WIRE;
			while (!tokens.empty())
			{
				wire.tokens.push_back(tokens.front());
				tokens.pop_front();
				if (wire.tokens.back().str == ";")
					break;
			}
			if (wire.tokens.back().str != ";")
			{
				std::cerr << "Look for ';' but reach the end" << std::endl;
				return false;
			}
			statements.push_back(wire);
		}
		else
		{
			evl_statement component;
			component.type = evl_statement::COMPONENT;
			while (!tokens.empty())
			{
				component.tokens.push_back(tokens.front());
				tokens.pop_front();
				if (component.tokens.back().str == ";")
					break;
			}
			if (component.tokens.back().str != ";")
			{
				std::cerr << "Look for ';' but reach the end" << std::endl;
				return false;
			}
			statements.push_back(component);
		}
	}
	return true;
}

bool netlist::create(const evl_wires &wires,
	const evl_components &comps,
	const evl_wires_table &wires_table)
{
	return create_nets(wires) && create_gates(comps, wires_table);
}

std::string make_net_name(std::string wire_name, int i)
{
	//assert(index >= 0);
	std::ostringstream oss;
	oss << wire_name << "[" << i << "]";
	return oss.str();
}

bool netlist::create_nets(const evl_wires &wires)
{
	for (evl_wires::const_iterator w = wires.begin();
		w != wires.end();
		w++)
	if ((*w).width == 1)
	{
		create_net((*w).name);
	}
	else
	{
		for (int i = 0; i < (*w).width; i++)
		{
			create_net(make_net_name((*w).name, i));
		}
	}
	return true;
}

void netlist::create_net(std::string net_name)
{
	assert(nets_table_.find(net_name) == nets_table_.end());
	net *n = new net;					
	n->net_name = net_name;				
	nets_table_[net_name] = n;						
	nets_.push_back(n);
}

bool netlist::create_gates(const evl_components &com,
	const evl_wires_table &wires_table)
{
	for (evl_components::const_iterator c = com.begin();
		c != com.end();
		c++)
	{
		create_gate((*c), wires_table);
	}
	return true;
}

bool netlist::create_gate(const evl_component &c,
	const evl_wires_table &wires_table)
{
	gate *g = new gate;				
	gates_.push_back(g);			
	return g->create(c, nets_table_, wires_table);
}

bool gate::create(const evl_component &c,
	const std::map<std::string, net *> &nets_table,
	const evl_wires_table &wires_table)
{
	type = c.type;
	name = c.name;
	size_t pin_index = 0;
	for (evl_pins::const_iterator ep = c.pins.begin();
		ep != c.pins.end();
		ep++)
	{
		create_pin((*ep), pin_index, nets_table, wires_table);
		++pin_index;
	}
	return true;
}

bool gate::create_pin(const evl_pin &ep,
	size_t pin_index,
	const std::map<std::string, net *> &nets_table,
	const evl_wires_table &wires_table)
{
	pin *p = new pin;
	pins_.push_back(p);
	return p->create(this, pin_index, ep, nets_table, wires_table);
}

bool pin::create(gate *g,
	size_t pin_index,
	const evl_pin &p,
	const std::map<std::string, net *> &nets_table,
	const evl_wires_table &wires_table)
{
	gate_ = g;
	pin_index_ = pin_index;
	std::string net_name;
	if (p.pin_msb == -1) 
	{
		if (wires_table.find(p.name)->second == 1)
		{
			net_name = p.name;
			net *n = new net;
			n = nets_table.find(net_name)->second;
			nets_.push_back(n);
			n->append_pin(this);
		}
		if (wires_table.find(p.name)->second != 1)
		{
			for (int i = 0;
				i <= (wires_table.find(p.name)->second) - 1;
				i++)
			{
				net_name = make_net_name(p.name, i);
				net *n = new net;
				n = nets_table.find(net_name)->second;
				nets_.push_back(n);
				n->append_pin(this);
			}
		}
	}
	else
	{
		if ((p.pin_msb != -1) && (p.pin_lsb != -1))
		{
			for (int i = p.pin_lsb; i <= p.pin_msb; i++)
			{
				net_name = make_net_name(p.name, i);
				net *n = new net;
				n = nets_table.find(net_name)->second;
				nets_.push_back(n);
				n->append_pin(this);
			}
		}
		if ((p.pin_msb != -1) && (p.pin_lsb == -1))
		{
			net_name = make_net_name(p.name, p.pin_msb);
			net *n = new net;
			n = nets_table.find(net_name)->second;
			nets_.push_back(n);
			n->append_pin(this);
		}
	}
	return true;
}

void net::append_pin(pin *p)
{
	connections_.push_back(p);
}

evl_wires_table make_wires_table(const evl_wires &wires)
{			
	evl_wires_table wires_table;
	for (evl_wires::const_iterator it = wires.begin();
		it != wires.end(); ++it)
	{
		evl_wires_table::iterator same_name = wires_table.find(it->name);
		if (same_name != wires_table.end())
		{
			std::cerr << "Wire '" << it->name
				<< "'is already defined" << std::endl;
		}
		wires_table[it->name] = it->width;
	}
	return wires_table;
}

void netlist::display(std::ostream &out, evl_statements &statements)
{
	for (evl_statements::const_iterator sta = statements.begin();
		sta != statements.end();
		++sta)
	{
		if ((*sta).type == evl_statement::MODULE)
		{
			for (evl_tokens::const_iterator  module_a = (*sta).tokens.begin();
				module_a != (*sta).tokens.end();
				++module_a)
			{
				if (module_a->str != ";")
					out << module_a->str << " ";
			}
			out << std::endl;
		}
	}
	out << "nets " << nets_.size() << std::endl;
	for (std::list<net *>::const_iterator n_ = nets_.begin();
		n_ != nets_.end();
		++n_)
	{
		(*n_)->display(out);
	}
	out << "components " << gates_.size() << std::endl;
	for (std::list<gate *>::const_iterator ga = gates_.begin();
		ga != gates_.end();
		++ga)
	{
		(*ga)->display(out);
	}
}

void net::display(std::ostream &out)
{
	out << "  net " << net_name << " " << connections_.size() << std::endl;
	for (std::list<pin *>::const_iterator pi_ = connections_.begin();
		pi_ != connections_.end();
		++pi_)
	{
		(*pi_)->display_for_net(out);
	}
}

void pin::display_for_net(std::ostream &out)
{
	(gate_)->dispay_for_type_name(out);
	out << " " << pin_index_ << std::endl;
}

void gate::display(std::ostream &out)
{
	if (name != "")
	{
		out << "  component " << type << " " << name << " " << pins_.size() << std::endl;
		for (std::vector<pin *>::const_iterator pn = pins_.begin();
			pn != pins_.end();
			++pn)
		{
			(*pn)->display_for_component(out);
		}
	}
	if (name == "")
	{
		out << "  component " << type << " " << pins_.size() << std::endl;
		for (std::vector<pin *>::const_iterator pn = pins_.begin();
			pn != pins_.end();
			++pn)
		{
			(*pn)->display_for_component(out);
		}
	}
}

void gate::dispay_for_type_name(std::ostream &out)
{
	if (name != "")
	{
		out << "    " << type << " " << name;
	}
	if (name == "")
	{
		out << "    " << type;
	}
}

void pin::display_for_component(std::ostream &out)
{
	out << "    pin " << nets_.size();
	for (std::vector<net *>::const_iterator ite = nets_.begin();
		ite != nets_.end();
		ite++)
	{
		out << " " << (*ite)->net_name;
	}
	out << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		std::cerr << "You should provide a file name." << std::endl;
		return -1;
	}
	std::string evl_file = argv[1];
	evl_tokens tokens;
	evl_wires wires;
	evl_statements statements;
	evl_components comps;
	if (!extract_tokens_from_file(evl_file, tokens))
	{
		return -1;
	}
	group_tokens_into_statements(statements, tokens);
	for (evl_statements::const_iterator sta = statements.begin();
		sta != statements.end();
		++sta)
	{
		if ((*sta).type == evl_statement::WIRE)			
		{
			evl_statement s = (*sta);
			if (!process_wire_statement(wires, s))
			{
				std::cerr << "process wire statement error"
					<< std::endl;
				break;
			}
		}
		else if ((*sta).type == evl_statement::COMPONENT)		
		{
			evl_statement s = (*sta);
			if (!process_component_statement(comps, s))
			{
				std::cerr << "process component statement error"
					<< std::endl;
				break;
			}
		}
	}
	evl_wires_table wires_table = make_wires_table(wires);
	netlist nl;
	if (!nl.create(wires, comps, wires_table))
	{
		return -1;
	}
	std::string file_name = std::string(argv[1]) + ".netlist";
	std::ofstream output_file(file_name.c_str());
	if (!output_file)
	{
		std::cerr << " I can't wire " << file_name << ".netlist" << std::endl;
		return -1;
	}
	nl.display(output_file, statements);
	return 0;
}






