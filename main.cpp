#include <algorithm>
#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

inline bool containes(std::string const& s, int c)
{
	return s.find(c) != std::string::npos;
}

struct chars_in_file
{
	int ch{ 0 };

	chars_in_file(std::ifstream& f) : file(f)
	{
	}

	int operator()()
	{
		if (ch != EOF)
		{
			ch = file.get();
		}
		return ch;
	}

	void put_back()
	{
		file.putback(ch);
	}

private:
	std::ifstream& file;
};

struct token
{
	std::string ch;
	std::string data;
	struct ast* n{ nullptr };

	std::pair<std::string, std::string> operator()() const
	{
		return { ch, data };
	}

	std::string to_string() const
	{
		return "{" + ch + ", " + data + "}";
	}
};

struct lex
{
	chars_in_file& chars;

	lex(chars_in_file& chars_iterator) : chars(chars_iterator)
	{
	}

	std::string scan_str(int delimiter) const
	{
		std::string res;
		while (chars() != delimiter && chars.ch != EOF)
		{
			res += chars.ch;
		}
		return res;
	}

	std::string scan(int i, int(*is_valid)(int), int valid_ch = EOF) const
	{
		std::string res;
		res += i;
		while ((is_valid(chars()) || chars.ch == valid_ch) && chars.ch != EOF)
		{
			res += chars.ch;
		}
		chars.put_back();
		return res;
	}

	token operator()() const
	{
		if (chars.ch != EOF)
		{
			chars();
			while (isspace(chars.ch) || chars.ch == '\t') chars();
			auto c = chars.ch;

			if (containes("(){},;=:\\", c)) return { {static_cast<char>(c)}, "" };
			if (c == '&' || c == '|') return { "operation0", {static_cast<char>(c)} };
			if (c == '<' || c == '>' || c == '~') return { "operation1", {static_cast<char>(c)} };
			if (c == '+' || c == '-') return { "operation2", {static_cast<char>(c)} };
			if (c == '*' || c == '/') return { "operation3", {static_cast<char>(c)} };
			if (c == '.') return { "operation4", {static_cast<char>(c)} };
			if (c == '\'' || c == '"') return { "string", scan_str(c) };
			if (isdigit(c)) return { "number", scan(c, isdigit, '.') };
			if (isalpha(c) || c == '_')
			{
				auto const& data = scan(c, isalnum, '_');
				return data == "if" || data == "else" || data == "for" || data == "while"
					? token{ data, "" }
				: token{ "symbol", data };
			}
		}
		return {};
	}
};


struct lex_buff
{
	lex& l;
	lex_buff(lex& lexer) : l(lexer)
	{
	}
	std::vector<token> buffer;
	token operator()()
	{
		token t;
		if (!buffer.empty())
		{
			t = buffer.back();
			buffer.pop_back();
		}
		else
		{
			t = l();
		}
		return t;
	}
	void push(token t)
	{
		buffer.push_back(t);
	}
	void push(std::vector<token> const& ts)
	{
		buffer.insert(std::end(buffer), std::rbegin(ts), std::rend(ts));
	}
};



struct ast
{
	enum class ast_type
	{
		operation,
		ite,
		for_loop,
		whl_loop,
		func,
		call,
		assign,
		body,
		symbol,
		number,
		string,
		data_num,
		data_str,
		data_obj,
		data_vec
	} type;

	ast(ast_type tp) : type(tp)
	{
	}

	virtual std::string to_string() = 0;
	virtual ~ast() = default;
};

struct sym : ast
{
	std::string s;
	sym(std::string const& s_) : ast(ast_type::symbol), s(s_)
	{
	}
	std::string to_string() override
	{
		return s;
	}
};

struct num : ast
{
	std::string n;
	num(std::string const& s_) : ast(ast_type::number), n(s_)
	{
	}
	std::string to_string() override
	{
		return n;
	}
};

struct str : ast
{
	std::string s;
	str(std::string const& s_) : ast(ast_type::string), s(s_)
	{
	}
	std::string to_string() override
	{
		return s;
	}
};

struct operation : ast
{
	std::string op;
	ast* l;
	ast* r;
	operation(std::string const& o, ast* left, ast* right = nullptr) : ast(ast_type::operation), op(o), l(left), r(right)
	{
	}
	std::string to_string() override
	{
		return "{ operation\n" + op + "\n" + l->to_string() + "\n" + r->to_string() + "\n}";
	}
};

struct ITE : ast
{
	ast* p;
	ast* t;
	ast* e;
	ITE(ast* prop, ast* then, ast* else_) : ast(ast_type::ite), p(prop), t(then), e(else_)
	{
	}
	std::string to_string() override
	{
		return "{ if\n" + p->to_string() + "\n" + "then\n" + t->to_string() + (e != nullptr ? "\n" + e->to_string() : "") + "\n}";
	}
};

struct for_loop : ast
{
	std::string i;
	ast* rng;
	ast* b;
	for_loop(std::string const& it, ast* range, ast* body_) : ast(ast_type::for_loop), i(it), rng(range), b(body_)
	{
	}
	std::string to_string() override
	{
		return "{ for\n" + i + "\n" + rng->to_string() + "\n" + b->to_string() + "\n}";
	}
};

struct whl_loop : ast
{
	ast* p;
	ast* b;
	whl_loop(ast* prop, ast* body_) : ast(ast_type::whl_loop), p(prop), b(body_)
	{
	}
	std::string to_string() override
	{
		return "{ while\n" + p->to_string() + "\n" + b->to_string() + "\n}";
	}
};

struct body_ : ast
{
	std::vector<ast*> stmts;
	body_() : ast(ast_type::body)
	{
	}
	std::string to_string() override
	{
		std::string res;
		for (auto stmt : stmts)
		{
			res += (res.empty() ? "" : "\n") + stmt->to_string();
		}
		return "[\n" + res + "\n]";
	}
};

struct func : ast
{
	std::vector<std::string> as;
	ast* b;
	func(ast* body_ = nullptr) : ast(ast_type::func), b(body_)
	{
	}
	std::string to_string() override
	{
		std::string args;
		for (auto a : as)
		{
			args += (args.empty() ? "" : ", ") + a;
		}
		return "{ func\n[" + args + "]\n" + (b == nullptr ? "[]" : b->to_string()) + "\n}";
	}
};

struct call_ : ast
{
	std::vector<ast*> as;
	ast* src{ nullptr };
	call_() : ast(ast_type::call)
	{
	}
	std::string to_string() override
	{
		std::string args;
		for (auto a : as)
		{
			args += "\n" + a->to_string();
		}
		return "{ call\n" + src->to_string() + "\n[" + args + "\n]\n}";
	}
};

struct assign : ast
{
	std::string id;
	ast* v;
	assign(std::string const& identifier, ast* val) : ast(ast_type::assign), id(identifier), v(val)
	{
	}
	std::string to_string() override
	{
		return "{ assign\n" + id + "\n" + v->to_string() + "\n}";
	}
};


struct par_res
{
	bool success;
	std::vector<struct ast*> result;
};

struct par
{
	static lex_buff* lex_b;
	static std::vector<par*> all_parsers;

	std::string id{};

	virtual par_res operator()()
	{
		return { false,{} };
	}

	static void remove_all()
	{
		for (auto p : all_parsers)
		{
			delete p;
		}
		all_parsers.clear();
	}
};
lex_buff* par::lex_b{ nullptr };
std::vector<par*> par::all_parsers;

struct atom : par
{
	std::string sym;
	bool has_cb{ false };
	std::function<void(std::vector<ast*>&, token&)> cb;
	atom(std::string const& s) : sym(s)
	{
		id = s;
		all_parsers.push_back(this);
	}
	atom(std::string const& s, std::function<void(std::vector<ast*>&, token&)> on_success) : sym(s), has_cb(true), cb(on_success)
	{
		id = s;
		all_parsers.push_back(this);
	}
	par_res operator()() override
	{
		auto t = (*lex_b)();
		par_res res{ t.ch == sym, {} };

		if (res.success)
		{
			if (has_cb) cb(res.result, t);
		}
		else
		{
			(*lex_b).push(t);
		}
		return res;
	}
};

struct all : par
{
	std::vector<par*> ps;
	std::function<void(std::vector<ast*>&)> cb;
	bool has_cb{ false };
	all(std::initializer_list<par*> p) : ps(p)
	{
		all_parsers.push_back(this);
	}
	all(std::initializer_list<par*> p, std::function<void(std::vector<ast*>&)> on_success) : ps(p), cb(on_success), has_cb(true)
	{
		all_parsers.push_back(this);
	}
	par_res operator()() override
	{
		par_res res{ true, {} };
		for (auto p : ps)
		{
			par_res sub_res = (*p)();

			if (!sub_res.success)
			{
				res.success = false;
				for (auto n : res.result)
				{
					delete n;
				}
				res.result.clear();
				break;
			}
			if (!sub_res.result.empty())
			{
				res.result.insert(
					std::end(res.result),
					std::begin(sub_res.result),
					std::end(sub_res.result));
			}
		}
		if (res.success && has_cb)
		{
			cb(res.result);
		}
		return res;
	}
};

struct any : par
{
	std::vector<par*> ps;
	std::function<void(std::vector<ast*>&)> cb;
	bool has_cb{ false };

	any(std::initializer_list<par*> p) : ps(p)
	{
		all_parsers.push_back(this);
	}
	any(std::initializer_list<par*> p, std::function<void(std::vector<ast*>&)> on_success) : ps(p), cb(on_success), has_cb(true)
	{
		all_parsers.push_back(this);
	}
	par_res operator()() override
	{
		par_res res{ false, {} };
		for (auto p : ps)
		{
			par_res sub_res = (*p)();
			if (sub_res.success)
			{
				res.success = true;
				res.result.insert(
					std::end(res.result),
					std::begin(sub_res.result),
					std::end(sub_res.result));
				break;
			}
		}
		if (res.success && has_cb)
		{
			cb(res.result);
		}
		return res;
	}
};

struct many : par
{
	par* sep;
	par* p;
	bool has_cb{ false };
	std::function<void(std::vector<ast*>&)> cb;
	many(par* s, par* pr) : sep(s), p(pr)
	{
		all_parsers.push_back(this);
	}
	many(par* s, par* pr, std::function<void(std::vector<ast*>&)> on_success) : sep(s), p(pr), has_cb(true), cb(on_success)
	{
		all_parsers.push_back(this);
	}
	par_res operator()() override
	{
		par_res res{ false, {} };
		bool odd = true;
		for (;;)
		{
			par_res sub_res = (odd ? *p : *sep)();
			if (!sub_res.success)
			{
				break;
			}
			res.success = true;
			if (odd)
			{
				res.result.insert(
					std::end(res.result),
					std::begin(sub_res.result),
					std::end(sub_res.result));
			}
			odd = !odd;
		}
		if (res.success && has_cb)
		{
			cb(res.result);
		}
		return res;
	}
};

struct sep_by : par
{
	par* sep;
	par* p;
	bool has_cb{ false };
	std::function<void(bool const, std::vector<ast*>&, std::vector<ast*>&)> cb;
	sep_by(par* s, par* pr) : sep(s), p(pr)
	{
		all_parsers.push_back(this);
	}
	sep_by(par* s, par* pr, std::function<void(bool const, std::vector<ast*>&, std::vector<ast*>&)> on_success) : sep(s), p(pr), has_cb(true), cb(on_success)
	{
		all_parsers.push_back(this);
	}
	par_res operator()() override
	{
		par_res res{ false, {} };
		bool odd = true;
		for (;;)
		{
			par_res sub_res = (odd ? *p : *sep)();
			if (!sub_res.success)
			{
				break;
			}
			res.success = true;
			if (has_cb)
			{
				cb(odd, res.result, sub_res.result);
			}
			odd = !odd;
		}
		if (res.success && odd)
		{
			res.success = false;
			for (auto n : res.result)
			{
				delete n;
			}
			res.result.clear();
		}
		return res;
	}
};

struct if_next : par
{
	atom* first;
	atom* second;
	if_next(atom* first_, atom* next) : first(first_), second(next)
	{
		all_parsers.push_back(this);
	}
	par_res operator()()
	{
		auto res = (*first)();
		if(res.success)
		{
			auto sub_res = (*second)();
			if (!sub_res.success)
			{
				push(sub_res.result);
				push(res.result);
				res.success = false;
			}
			else if (!sub_res.result.empty())
			{
				res.result.push_back(sub_res.result.back());
			}
		}
		return res;
	}
	void push(std::vector<ast*>& as)
	{
		if (!as.empty())
		{
			auto n = as.back();
			as.pop_back();
			switch (n->type)
			{
			case ast::ast_type::symbol:
				lex_b->push(token{ "symbol", reinterpret_cast<sym*>(n)->s });
				break;
			case ast::ast_type::string:
				lex_b->push(token{ "string", reinterpret_cast<str*>(n)->s });
				break;
			case ast::ast_type::number:
				lex_b->push(token{ "number", reinterpret_cast<num*>(n)->n });
				break;
			default:
				break;
			}
			delete n;
		}
	}
};

struct opt : par
{
	par* p;
	opt(par* pr) : p(pr)
	{
		all_parsers.push_back(this);
	}
	par_res operator()() override
	{
		return { true, (*p)().result };
	}
};

struct parser
{
	par* p;
	parser(lex_buff& l)
	{
		par::lex_b = &l;
		auto add_operator = [](bool const odd, std::vector<ast*>& as, std::vector<ast*>& n)
		{
			if (odd)
			{
				if (as.empty())
				{
					as.push_back(n.back());
				}
				else
				{
					static_cast<operation*>(as.back())->r = n.back();
				}
			}
			else
			{
				as.back() = new operation{ reinterpret_cast<sym*>(n.back())->s, as.back() };
				delete n.back();
				n.pop_back();
			}
		};
		auto add_body = [](std::vector<ast*>& as)
		{
			auto b = new body_();
			b->stmts = as;
			as = { b };
		};
		auto add_func = [](std::vector<ast*>& as)
		{
			auto f = new func{};
			if (!as.empty() && as.back()->type == ast::ast_type::body)
			{
				f->b = as.back();
				as.pop_back();
			}
			for (auto a : as)
			{
				f->as.push_back(reinterpret_cast<sym*>(a)->s);
				delete a;
			}
			as = { f };
		};
		auto add_call = [](std::vector<ast*>& as)
		{
			auto c = new call_{};
			c->as = as;
			as = { c };
		};
		auto complete_call = [](std::vector<ast*>& as)
		{
			if (as.back()->type == ast::ast_type::call)
			{
				static_cast<call_*>(as.back())->src = as[0];
				as[0] = as.back();
				as.pop_back();
			}
		};
		auto add_assignment = [](std::vector<ast*>& as)
		{
			auto s = reinterpret_cast<sym*>(as[0]);
			as = { new assign(s->s, as[1]) };
			delete s;
		};
		auto add_ite = [](std::vector<ast*>& as)
		{
			// if [0]()  [1]{}  else  [2]{}
			as = { new ITE{ as[0], as[1], 2 < as.size() ? as[1] : nullptr } };
		};
		auto add_for = [](std::vector<ast*>& as)
		{
			// for [0]i : [1]range [2]{}
			sym* i = reinterpret_cast<sym*>(as[0]);
			as = { new for_loop{ i->s, as[1], as[2] } };
			delete i;
		};
		auto add_whl = [](std::vector<ast*>& as)
		{
			as = { new whl_loop{ as[0], as[1] } };
		};
		auto add_sym = [](std::vector<ast*>& as, token& tok)
		{
			as.push_back(new sym(tok.data));
		};
		auto add_num = [](std::vector<ast*>& as, token& tok)
		{
			as.push_back(new num(tok.data));
		};
		auto add_str = [](std::vector<ast*>& as, token& tok)
		{
			as.push_back(new str(tok.data));
		};
		atom* symbol = _("symbol", add_sym);
		any* term = new any{ symbol,_("number",add_num),_("string", add_str) };
		all* call = new all({ _("(") }, add_call);
		par* sub_expr4 = new sep_by(_("operation4", add_sym), term,     /**/ add_operator);
		par* sub_expr3 = new sep_by(_("operation3", add_sym), sub_expr4,/**/ add_operator);
		par* sub_expr2 = new sep_by(_("operation2", add_sym), sub_expr3,/**/ add_operator);
		par* sub_expr1 = new sep_by(_("operation1", add_sym), sub_expr2,/**/ add_operator);
		all* body = new all{ _("{") };
		all* lambda = new all({ new opt{new all{_("\\"), new opt{new many{_(","),symbol}}}}, body }, add_func);
		par* expr = new any{ new all({new sep_by{ _("operation0", add_sym), sub_expr1,/**/ add_operator }, new opt{call}}, complete_call), lambda };
		term->ps.push_back(new all({ _("(") , expr,_(")") }));
		call->ps.push_back(new any{ _(")"), new all{new many{_(","), expr},_(")")} });

		par* assign = new all({ new if_next{symbol,_("=")}, expr }, add_assignment);
		par* ite = new all({ _("if"), expr, body, new opt{new all{_("else"), body}} }, add_ite);
		par* for_cycle = new all({ _("for"), new opt(_("(")), symbol,_(":"), expr, new opt(_(")")), body }, add_for);
		par* whl_cycle = new all({ _("while"), expr, body }, add_whl);
		par* stmts = new many(_(";"), new any{ ite, for_cycle, whl_cycle, assign, expr },/**/ add_body);
		body->ps.push_back(new opt{ stmts });
		body->ps.push_back(_("}"));

		p = stmts;

		// debug data
		term->id = "term";
		sub_expr1->id = "sub_expr1";
		sub_expr2->id = "sub_expr2";
		sub_expr3->id = "sub_expr3";
		sub_expr4->id = "sub_expr4";
		body->id = "body";
		lambda->id = "lambda";
		expr->id = "expr";
		call->id = "call";
		assign->id = "assign";
		ite->id = "ite";
		for_cycle->id = "for_cycle";
		whl_cycle->id = "whl_cycle";
		stmts->id = "stmts";
	}

	~parser()
	{
		par::remove_all();
	}

	par_res operator()()
	{
		return (*p)();
	}
	
	static atom* _(std::string const& s)
	{
		return new atom{ s };
	}
	static atom* _(std::string const& s, std::function<void(std::vector<ast*>&, token&)> f)
	{
		return new atom{ s, f };
	}
};

struct data_num : ast
{
	float value;
	data_num(float n) : ast(ast_type::data_num), value(n)
	{
	}
	std::string to_string() override
	{
		return floorf(value) == value ? std::to_string(static_cast<int>(value)) : std::to_string(value);
	}
};

struct data_str : ast
{
	std::string value;
	data_str(std::string s) : ast(ast_type::data_str), value(s)
	{
	}
	std::string to_string() override
	{
		return value;
	}
};

struct data_obj : ast
{
	std::map<std::string, ast*> value;
	data_obj(std::map<std::string, ast*> const& o) : ast(ast_type::data_obj), value(o)
	{
	}
	std::string to_string() override
	{
		std::string res = "{";
		bool first = true;
		for (auto& [key, val] : value)
		{
			if (first) first = false;
			else res += ", ";
			res += key + ": " + (val == nullptr ? "null" : val->to_string());
		}
		return res + "}";
	}
};

struct data_vec : ast
{
	std::vector<ast*> value;
	data_vec(std::vector<ast*> const& v) : ast(ast_type::data_vec), value(v)
	{
	}
	std::string to_string() override
	{
		std::string res = "[";
		bool first = true;
		for (auto val : value)
		{
			if (first) first = false;
			else res += ", ";
			res += val == nullptr ? "null" : val->to_string();
		}
		return res + "]";
	}
};

struct scope
{
	scope() = default;
	
	scope(scope const& s) : scp(s.scp)
	{
	}
	scope clone()
	{
		return scope(*this);
	}
	void set(std::string const& key, ast* value)
	{
		scp[key] = value;
	}
	ast* get(std::string const& key)
	{
		return scp[key];
	}

	bool has(std::string const& key)
	{
		return scp.count(key);
	}

private:
	std::map<std::string, ast*> scp;
};

struct block_context
{
	std::string module_name;
	scope block_scope;
	bool return_called{ false };
	bool break_called{ false };
	bool continue_called{ false };
	ast* return_object{ nullptr };
};

struct evaluator
{
	void run_func(func* f, const std::vector<ast*>& as)
	{
		ctx.push_back({});
		
		for (size_t i = 0; i < as.size(); ++i)
		{
			set(f->as[i], as[i]);
		}
		eval(f->b);
		if (ctx.back().return_object)
		{
			auto const ret = ctx.back().return_object;
			ctx.pop_back();
			if (!ctx.empty())
			{
				auto& back = ctx.back();
				back.return_object = ret;
				back.return_called = true;
			}
		}
	}

	void eval(ast* node)
	{
		switch (node->type)
		{
		case ast::ast_type::assign:
			if (!ctx.empty())
			{
				auto a = reinterpret_cast<assign*>(node);
				eval(a->v);
				set(a->id, ctx.back().return_object);
				ctx.back().return_object = nullptr;
			}
			break;
		case ast::ast_type::body:
			{
				ctx.push_back({});
				auto& stmts = reinterpret_cast<body_*>(node)->stmts;
				auto const sz = stmts.size();
				for (size_t i = 0; i < sz; ++i)
				{
					eval(stmts[i]);
					auto& ctx_back = ctx.back();
					if (ctx_back.break_called)
					{
						break;
					}
					if (ctx_back.return_called || i == sz - 1)
					{
						auto rt = ctx_back.return_object;
						ctx.pop_back();
						ctx.back().return_object = rt;
						break;
					}
					delete ctx_back.return_object;
					ctx_back.return_object = nullptr;
				}
			}
			break;
		case ast::ast_type::call:
			{
				auto expr = reinterpret_cast<call_*>(node);
				eval(expr->src);
				auto f = reinterpret_cast<func*>(ctx.back().return_object);
				std::vector<ast*> as;
				for (auto a : expr->as)
				{
					eval(a);
					as.push_back(ctx.back().return_object);
				}
				run_func(f, as);
			}
			break;
		case ast::ast_type::for_loop:
			{
				auto fr = reinterpret_cast<for_loop*>(node);
				eval(fr->rng);
				auto rng = reinterpret_cast<data_vec*>(ctx.back().return_object);
				ctx.back().return_object = nullptr;
				
				for (auto r : rng->value)
				{
					ctx.back().block_scope.set(fr->i, r);
					eval(fr->b);
					if (auto& back = ctx.back(); back.break_called || back.return_called)
					{
					}
				}
			}
			break;
		case ast::ast_type::whl_loop:
			{
			}
			break;
		case ast::ast_type::func:
			{
				ctx.back().return_object = node;
			}
			break;
		case ast::ast_type::ite:
			{
			}
			break;
		case ast::ast_type::number:
			{
				ctx.back().return_object = new data_num(atof(reinterpret_cast<num*>(node)->n.c_str()));
			}
			break;
		case ast::ast_type::string:
			{
				ctx.back().return_object = new data_str(reinterpret_cast<str*>(node)->s);
			}
			break;
		case ast::ast_type::symbol:
			{
				if (auto e = get(reinterpret_cast<sym*>(node)->s); e != nullptr)
				{
					eval(e);
				}
			}
			break;
		case ast::ast_type::operation:
			{
				auto o = reinterpret_cast<operation*>(node);
				eval(o->l);
				auto l = ctx.back().return_object;
				eval(o->r);
				auto r = ctx.back().return_object;
				ctx.back().return_object = nullptr;
				
				if (l->type == r->type)
				{
					if (l->type == ast::ast_type::data_num)
					{
						ctx.back().return_object = l;
						if      (o->op == "+") reinterpret_cast<data_num*>(l)->value += reinterpret_cast<data_num*>(r)->value;
						else if (o->op == "-") reinterpret_cast<data_num*>(l)->value -= reinterpret_cast<data_num*>(r)->value;
						else if (o->op == "*") reinterpret_cast<data_num*>(l)->value *= reinterpret_cast<data_num*>(r)->value;
						else if (o->op == "/") reinterpret_cast<data_num*>(l)->value /= reinterpret_cast<data_num*>(r)->value;
						delete r;
					}
					else if (l->type == ast::ast_type::data_str)
					{
						ctx.back().return_object = l;
						if (o->op == "+") reinterpret_cast<data_str*>(l)->value += reinterpret_cast<data_str*>(r)->value;
						delete r;
					}
				}
				else
				{
					if (l->type == ast::ast_type::data_str && r->type == ast::ast_type::data_num)
					{
						ctx.back().return_object = l;
						if (o->op == "+") reinterpret_cast<data_str*>(l)->value += reinterpret_cast<data_num*>(r)->to_string();
						delete r;
					}
					if (r->type == ast::ast_type::data_str && l->type == ast::ast_type::data_num)
					{
						ctx.back().return_object = r;
						auto s = reinterpret_cast<data_str*>(r);
						if (o->op == "+") s->value = reinterpret_cast<data_num*>(l)->to_string() + s->value;
						delete l;
					}
				}
			}
			break;
		case ast::ast_type::data_num:
			ctx.back().return_object = new data_num(reinterpret_cast<data_num*>(node)->value);
			break;
		case ast::ast_type::data_obj:
			break;
		case ast::ast_type::data_str:
			ctx.back().return_object = new data_str(reinterpret_cast<data_str*>(node)->value);
			break;
		case ast::ast_type::data_vec:
			break;
		default:
			break;
		}
	}

	std::vector<block_context> ctx;

private:
	
	ast* get(std::string const& key)
	{
		if (ctx.empty()) add_module("main");
		ast* res = nullptr;
		for (size_t i = ctx.size() - 1; ; --i)
		{
			if (ctx[i].block_scope.has(key))
			{
				res = ctx[i].block_scope.get(key);
				break;
			}
			if (i == 0)
			{
				break;
			}
		}
		return res;
	}
	void set(std::string const& key, ast* value)
	{
		if (ctx.empty()) add_module("main");
		ctx.back().block_scope.set(key, value);
	}
	void add_module(std::string const& module_name)
	{
		ctx.push_back(block_context{ module_name });
	}
};

int main()
{
	//auto env = new Env(std::cin, std::cout, std::cerr);
	if (std::ifstream file("main.txt"); file.is_open())
	{
		chars_in_file chars(file);
		lex lexer(chars);
		lex_buff lexer_b(lexer);
		parser p(lexer_b);

		par_res res = p();
		std::cout << res.success << std::endl;

		if (res.success && !res.result.empty())
		{
			evaluator ev;
			ev.ctx.push_back({"main"});
			ev.eval(res.result.back());
			if (!ev.ctx.empty())
			{
				std::cout << ev.ctx.back().return_object->to_string() << std::endl;
			}
			//std::cout << "\n" << res.result.back()->to_string() << std::endl;
		}

		//eval_list(parse(lexer));
	}

	return 0;
}
