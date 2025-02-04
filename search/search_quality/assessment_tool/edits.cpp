#include "search/search_quality/assessment_tool/edits.hpp"

#include "base/assert.hpp"

// Edits::RelevanceEditor --------------------------------------------------------------------------
Edits::RelevanceEditor::RelevanceEditor(Edits & parent, size_t index)
  : m_parent(parent), m_index(index)
{
}

bool Edits::RelevanceEditor::Set(Relevance relevance)
{
  return m_parent.UpdateRelevance(m_index, relevance);
}

Edits::Relevance Edits::RelevanceEditor::Get() const
{
  auto const & relevances = m_parent.GetRelevances();
  CHECK_LESS(m_index, relevances.size(), ());
  return relevances[m_index];
}

// Edits -------------------------------------------------------------------------------------------
Edits::Edits(Observer & observer) : m_observer(observer) {}

void Edits::ResetRelevances(std::vector<Relevance> const & relevances)
{
  WithObserver([this, &relevances]() {
    m_origRelevances = relevances;
    m_currRelevances = relevances;
    m_numEdits = 0;
  });
}

bool Edits::UpdateRelevance(size_t index, Relevance relevance)
{
  return WithObserver([this, index, relevance]() {
    CHECK_LESS(index, m_currRelevances.size(), ());
    CHECK_EQUAL(m_currRelevances.size(), m_origRelevances.size(), ());

    if (m_currRelevances[index] != m_origRelevances[index] && relevance == m_origRelevances[index])
    {
      --m_numEdits;
    }
    else if (m_currRelevances[index] == m_origRelevances[index] &&
             relevance != m_origRelevances[index])
    {
      ++m_numEdits;
    }

    m_currRelevances[index] = relevance;
    return m_currRelevances[index] != m_origRelevances[index];
  });
}

void Edits::Clear()
{
  WithObserver([this]() {
    m_origRelevances.clear();
    m_currRelevances.clear();
    m_numEdits = 0;
  });
}

bool Edits::HasChanges() const { return m_numEdits != 0; }
