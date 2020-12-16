
#include <doxmlintf.h>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <regex>
#include <filesystem>

namespace doxy2plantuml {

class Plant_UML_Writer {
public:
	void Start() {
		if (started) throw std::runtime_error("Builder already began");

		started = true;

		plant_uml_file.open("./output.txt", std::fstream::out);
		plant_uml_file << "@startuml" << std::endl;
	}

	void Start_Namespace(const char * namespace_name) {
		plant_uml_file << "namespace " << namespace_name << "{" << std::endl;
	}

	void End_Namespace() {
		plant_uml_file << "}" << std::endl;
	}

	void Start_Class(const char * class_id, const char * class_name) {
		plant_uml_file << "class " << class_name << " {" << std::endl;
	}

	void End_Class() {
		plant_uml_file << "}" << std::endl;
	}

	void Write_Relationship(const char * class_id_user, const char * class_id_used, IChildNode::NodeRelation relation) {
		switch (relation) {
			case IChildNode::PublicInheritance:
				plant_uml_file << class_id_user << " --|> " << class_id_used << std::endl;
				break;
			case IChildNode::ProtectedInheritance:
				plant_uml_file << class_id_user << " --|> " << class_id_used << std::endl;
				break;
			case IChildNode::PrivateInheritance:
				plant_uml_file << class_id_user << " --|> " << class_id_used << std::endl;
				break;
			case IChildNode::Usage:
				plant_uml_file << class_id_user << " --> " << class_id_used << std::endl;
				break;
			case IChildNode::TemplateInstance:
				break;
		}
	}

	void Write_Field(const char * class_name, const char * field_type, const char * field_name, const char * protection) {
		plant_uml_file << "\t";
		if (strcmp(protection, "public") == 0) {
			plant_uml_file << "+";
		} else if (strcmp(protection, "private") == 0) {
			plant_uml_file << "-";
		} else if (strcmp(protection, "protected") == 0) {
			plant_uml_file << "#";
		}
		plant_uml_file << field_name << " : " << field_type << std::endl;
	}

	void Write_Method(const char * class_name, const char * method_name,  const char * protection) {
		plant_uml_file << "\t";
		if (strcmp(protection, "public") == 0) {
			plant_uml_file << "+";
		} else if (strcmp(protection, "private") == 0) {
			plant_uml_file << "-";
		} else if (strcmp(protection, "protected") == 0) {
			plant_uml_file << "#";
		}
		plant_uml_file << method_name << "()" << std::endl;
	}

	void End() {
		if (!started) throw std::runtime_error("Builder not began.");

		started = false;

		plant_uml_file << "@enduml";
		plant_uml_file.close();
	}
private:
	bool started;
	std::fstream plant_uml_file;
} Plant_UML_Writer;

class Compound_Register {
public:
	void Add(std::string compound_id) {
		data[compound_id] = true;
	}
	bool Already_Registered(std::string compound_id) const {
		return data.contains(compound_id);
	}
private:
	std::map<std::string, bool> data;
} Compound_Register;

class Collaboration_Register {
public:
	void Add(std::string class_a, std::string class_b) {
		data[class_a].push_back(class_b);
	}
	bool Already_Registered(std::string class_a, std::string class_b) {
		if (!data.contains(class_a)) return false;
		for (auto & str : data.at(class_a)) {
			if (class_b == str) {
				return true;
			}
		}
		return false;
	}
private:
	std::map<std::string, std::vector<std::string>> data;
} Collaboration_Register;

class Inheritance_Register {
public:
	void Add(std::string class_a, std::string class_b) {
		data[class_a].push_back(class_b);
	}
	bool Already_Registered(std::string class_a, std::string class_b) {
		if (!data.contains(class_a)) return false;
		for (auto & str : data.at(class_a)) {
			if (class_b == str) {
				return true;
			}
		}
		return false;
	}
private:
	std::map<std::string, std::vector<std::string>> data;
} Inheritance_Register;

class Class_Name_Cleaner {
public:
	std::string Clean(const char * class_name) {
		std::string new_class_name = class_name;
		new_class_name = std::regex_replace(new_class_name, std::regex("::"), ".");
		return new_class_name;
	}
} Class_Name_Cleaner;

class Function_Processor {
public:
	void Run(IClass * cl, ISection * section, IFunction * func) {
		//std::cout << "Processing function: " << func->name()->latin1() << std::endl;

		Plant_UML_Writer.Write_Method(cl->name()->utf8(), func->name()->utf8(), func->protection()->utf8());
	}
} Function_Processor;

class Variable_Processor {
public:
	void Run(IClass * cl, ISection * section, IVariable * var) {
		//std::cout << "Processing varaible: " << var->name()->latin1() << " has protection " << var->protection()->utf8() << std::endl;

		Plant_UML_Writer.Write_Field(
			cl->name()->utf8(),
			var->typeString()->utf8(),
			var->name()->utf8(),
			var->protection()->utf8());
	}
} Variable_Processor;

class Member_Processor {
public:
	void Run(IClass * cl, ISection * section, IMember * member) {
		//std::cout << "Processing member: " << member->kindString()->latin1() << std::endl;

		if (member->kind() == IMember::MemberKind::Function) {
			Function_Processor.Run(cl, section, (IFunction *)member);
		} else if (member->kind() == IMember::MemberKind::Variable) {
			Variable_Processor.Run(cl, section, (IVariable *)member);
		}
	}
} Member_Processor;

class Section_Processor {
public:
	void Run(IClass * cl, ISection * section) {
		//std::cout << "Processing section: " << section->kindString()->latin1() << std::endl;

		IMemberIterator * member_iter = section->members();
		IMember * current_member;
		current_member = member_iter->current();
		while (current_member) {
			Member_Processor.Run(cl, section, current_member);

			member_iter->toNext();
			current_member = member_iter->current();
		}
		member_iter->release();
	}
} Section_Processor;

class Inheritance_Graph_Processor {
public:
	void Run(IGraph * inheritance_graph) {
		INodeIterator * node_iter = inheritance_graph->nodes();
		INode * current_node;
		current_node = node_iter->current();
		while (current_node) {
			//std::cout << "inhertiance graph node: " << current_node->label()->utf8() << std::endl;

			IChildNodeIterator * child_node_iterator = current_node->children();
			IChildNode * child_node;
			child_node = child_node_iterator->current();
			while (child_node) {
				//std::cout << "\t" << "Child node: " << child_node->node()->label()->utf8() << std::endl;

				std::string cleaned_class_name_derived = Class_Name_Cleaner.Clean(current_node->label()->utf8());
				std::string cleaned_class_name_base = Class_Name_Cleaner.Clean(child_node->node()->label()->utf8());

				if (!Inheritance_Register.Already_Registered(cleaned_class_name_derived, cleaned_class_name_base)) {
					Inheritance_Register.Add(cleaned_class_name_derived, cleaned_class_name_base);

					Plant_UML_Writer.Write_Relationship(cleaned_class_name_derived.c_str(), cleaned_class_name_base.c_str(), child_node->relation());
				}
				child_node_iterator->toNext();
				child_node = child_node_iterator->current();
			}
			child_node_iterator->release();

			/*
			Plant_UML_Writer.Write_Relationship_Usage(cleaned_class_name_client.c_str(), cleaned_class_name_server.c_str());
*/
			node_iter->toNext();
			current_node = node_iter->current();
		}
		node_iter->release();
	}
} Inheritance_Graph_Processor;

class Collaboration_Graph_Processor {
public:
	void Run(IGraph * collaboration_graph) {
		// get a list of all classes that connect to this one

		// The collaboration graph is a completely seperate entity to the
		// iterated heirachy. It is the collaboration from this point.

		INodeIterator * node_iter = collaboration_graph->nodes();
		INode * current_node;
		current_node = node_iter->current();
		while (current_node) {
			//std::cout << "Collab graph node: " << current_node->label()->utf8() << std::endl;

			IChildNodeIterator * child_node_iterator = current_node->children();
			IChildNode * child_node;
			child_node = child_node_iterator->current();
			while (child_node) {
				//std::cout << "\t" << "Child node: " << child_node->node()->label()->utf8() << std::endl;

				std::string cleaned_class_name_user = Class_Name_Cleaner.Clean(current_node->label()->utf8());
				std::string cleaned_class_name_used = Class_Name_Cleaner.Clean(child_node->node()->label()->utf8());

				if (!Collaboration_Register.Already_Registered(cleaned_class_name_user, cleaned_class_name_used)) {
					Collaboration_Register.Add(cleaned_class_name_user, cleaned_class_name_used);

					Plant_UML_Writer.Write_Relationship(cleaned_class_name_user.c_str(), cleaned_class_name_used.c_str(), child_node->relation());
				}
				child_node_iterator->toNext();
				child_node = child_node_iterator->current();
			}
			child_node_iterator->release();

			node_iter->toNext();
			current_node = node_iter->current();
		}
		node_iter->release();
	}
} Collaboration_Graph_Processor;

class Class_Processor {
public:
	void Run(IClass * cl) {
		// std::cout << "Processing class: " << cl->name()->latin1() << std::endl;

		// skip if already added from namespaces
		if (Compound_Register.Already_Registered(cl->id()->utf8())) {
			return;
		} else {
			Compound_Register.Add(cl->id()->utf8());
		}


		std::string class_name = Class_Name_Cleaner.Clean(cl->name()->utf8());// convert :: to . for plantuml namespaces
		Plant_UML_Writer.Start_Class(cl->id()->utf8(), class_name.c_str());

		ISectionIterator * section_iter = cl->sections();
		ISection * current_section;
		current_section = section_iter->current();
		while (current_section) {
			Section_Processor.Run(cl, current_section);

			section_iter->toNext();
			current_section = section_iter->current();
		}
		section_iter->release();

		Plant_UML_Writer.End_Class();


		IGraph * collaboration_graph = cl->collaborationGraph();
		if (collaboration_graph != nullptr) {
			Collaboration_Graph_Processor.Run(collaboration_graph);
		}

		IGraph * inheritance_graph = cl->inheritanceGraph();
		if (inheritance_graph != nullptr) {
			//Inheritance_Graph_Processor.Run(inheritance_graph);
		}
	}
} Class_Processor;

class Namespace_Processor {
public:
	void Run(INamespace * name_space) {
		ICompoundIterator * compound_iter = name_space->nestedCompounds();

		ICompound * current_compound;
		current_compound = compound_iter->current();
		while (current_compound) {

			// recursive namespaces
			if (current_compound->kind() == ICompound::CompoundKind::Namespace) {
				Run((INamespace *)current_compound);
			} else if (current_compound->kind() == ICompound::CompoundKind::Class) {
				Class_Processor.Run((IClass *)current_compound);
			}

			compound_iter->toNext();
			current_compound = compound_iter->current();
		}

		compound_iter->release();
	}
} Namespace_Processor;

class Classes_Iterator {
public:
	void Run(IDoxygen * doxygen) {
		ICompoundIterator * iter = doxygen->compounds();
		ICompound * current_compound;
		current_compound = iter->current();
		while (current_compound) {

			if (current_compound->kind() == ICompound::CompoundKind::Class) {
				Class_Processor.Run((IClass *)current_compound);
			}

			iter->toNext();
			current_compound = iter->current();
		}
		iter->release();
	}
} Classes_Iterator;

class Namespaces_Iterator {
public:
	void Run(IDoxygen * doxygen) {
		ICompoundIterator * iter = doxygen->compounds();
		ICompound * current_compound;
		current_compound = iter->current();
		while (current_compound) {

			if (current_compound->kind() == ICompound::CompoundKind::Namespace) {
				Namespace_Processor.Run((INamespace *)current_compound);
			}

			iter->toNext();
			current_compound = iter->current();
		}
		iter->release();
	}
} Namespaces_Iterator;

class Compound_Iterator {
public:
	void Run(IDoxygen * doxygen) {
		// namespaces first
		Namespaces_Iterator.Run(doxygen);

		// classes afterwards to avoid duplicates
		Classes_Iterator.Run(doxygen);
	}
} Compound_Iterator;

}

int main(int argc, char** argv) {
	std::cout << "doxy2plantuml: started" << std::endl;

	IDoxygen * doxygen = createObjectModel();

	std::string cwd;

	if (argc > 1) {
		cwd = std::filesystem::path(argv[1]);
	} else {
		cwd = std::filesystem::path(argv[0]).parent_path();
	}

	std::string xml_dir = (cwd + "/xml/");

	if (!doxygen->readXMLDir(xml_dir.c_str())) {
		throw std::runtime_error(xml_dir + " not found");
	}

	// Begin the writer
	doxy2plantuml::Plant_UML_Writer.Start();

	// Iterate compounds
	doxy2plantuml::Compound_Iterator.Run(doxygen);

	// End writing
	doxy2plantuml::Plant_UML_Writer.End();

	doxygen->release();


	std::cout << "doxy2plantuml: done" << std::endl;

	return 0;
}
