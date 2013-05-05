#include "Section.hpp"

Section::Section(eSection _section)
{
	this->section = _section;
}

Section::~Section()
{
  std::list<Object *>::iterator it;
  this->DeleteObjects();
}

// Members functions
void Section::AddObject(Object *item)
{
	this->objects.push_back(item);
}

void Section::AddObjects(const std::list<Object *items> &)
{
  std::list<Object *>::iterator it;

  it = items.begin();
  while (it != items.end())
	{
	  this->objects.push_back(*it);
      it++;
    }	
}

void Section::DeleteObjects(void)
{
	std::list<Object *>::iterator it;

	it = this->objects.begin();
	while (it != this->objects.end())
	{
      delete (*it);
      it++;
    }	
}

void Section::ClearObjects(void)
{
	this->objects.clear();
}

void Section::DrawObjects(void)
{
	std::list<Object *>::iterator it;

	it = this->objects.begin();
	while (it != this->objects.end())
	{
      (*it)->Draw();
      it++;
    }	
}

void Section::UpdateObjects(void)
{
	std::list<Object *>::iterator it;

	it = this->objects.begin();
	while (it != this->objects.end())
	{
      (*it)->Update();
      it++;
    }	
}

// Getters
eSection Section::getSection(void) const
{
	return (this->section);
}

// Setters
void Section::setSection(eSection value)
{
	this->section = value;
}