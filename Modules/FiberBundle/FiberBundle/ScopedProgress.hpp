#ifndef IMEKA_SCOPED_PROGRESS_HPP_INCLUDED
#define IMEKA_SCOPED_PROGRESS_HPP_INCLUDED

#include <algorithm>

#include <mitkProgressBar.h>

// Drives the MITK status bar progress bar for a known number of steps.
//
// The remaining steps are completed on destruction, so the progress bar is
// always hidden at the end, even if the operation throws midway.
class ScopedProgress
{
public:
  explicit ScopedProgress(unsigned int nbSteps)
    : m_NbSteps(nbSteps)
  {
    mitk::ProgressBar::GetInstance()->Reset();
    mitk::ProgressBar::GetInstance()->AddStepsToDo(m_NbSteps);
  }

  ~ScopedProgress()
  {
    AdvanceTo(m_NbSteps);
  }

  ScopedProgress(const ScopedProgress&) = delete;
  ScopedProgress& operator=(const ScopedProgress&) = delete;

  void Advance(unsigned int nbSteps = 1)
  {
    AdvanceTo(m_Done + nbSteps);
  }

  // Never goes backward nor past the total number of steps
  void AdvanceTo(unsigned int done)
  {
    done = std::min(done, m_NbSteps);
    if (done > m_Done)
    {
      mitk::ProgressBar::GetInstance()->Progress(done - m_Done);
      m_Done = done;
    }
  }

private:
  const unsigned int m_NbSteps;
  unsigned int m_Done = 0;
};

#endif // IMEKA_SCOPED_PROGRESS_HPP_INCLUDED
