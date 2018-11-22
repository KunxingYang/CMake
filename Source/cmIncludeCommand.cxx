/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmIncludeCommand.h"

#include <sstream>

#include "cmGlobalGenerator.h"
#include "cmMakefile.h"
#include "cmMessageType.h"
#include "cmPolicies.h"
#include "cmSystemTools.h"

class cmExecutionStatus;

// cmIncludeCommand
bool cmIncludeCommand::InitialPass(std::vector<std::string> const& args,
                                   cmExecutionStatus&)
{
  if (args.empty() || args.size() > 4) {
    this->SetError("called with wrong number of arguments.  "
                   "include() only takes one file.");
    return false;
  }
  bool optional = false;
  bool noPolicyScope = false;
  std::string fname = args[0];
  std::string resultVarName;

  for (unsigned int i = 1; i < args.size(); i++) {
    if (args[i] == "OPTIONAL") {
      if (optional) {
        this->SetError("called with invalid arguments: OPTIONAL used twice");
        return false;
      }
      optional = true;
    } else if (args[i] == "RESULT_VARIABLE") {
      if (!resultVarName.empty()) {
        this->SetError("called with invalid arguments: "
                       "only one result variable allowed");
        return false;
      }
      if (++i < args.size()) {
        resultVarName = args[i];
      } else {
        this->SetError("called with no value for RESULT_VARIABLE.");
        return false;
      }
    } else if (args[i] == "NO_POLICY_SCOPE") {
      noPolicyScope = true;
    } else if (i > 1) // compat.: in previous cmake versions the second
                      // parameter was ignored if it wasn't "OPTIONAL"
    {
      std::string errorText = "called with invalid argument: ";
      errorText += args[i];
      this->SetError(errorText);
      return false;
    }
  }

  if (fname.empty()) {
    this->Makefile->IssueMessage(MessageType::AUTHOR_WARNING,
                                 "include() given empty file name (ignored).");
    return true;
  }

  if (!cmSystemTools::FileIsFullPath(fname)) {
    // Not a path. Maybe module.
    std::string module = fname;
    module += ".cmake";
    std::string mfile = this->Makefile->GetModulesFile(module.c_str());
    if (!mfile.empty()) {
      fname = mfile;
    }
  }

  std::string fname_abs = cmSystemTools::CollapseFullPath(
    fname, this->Makefile->GetCurrentSourceDirectory());

  cmGlobalGenerator* gg = this->Makefile->GetGlobalGenerator();
  if (gg->IsExportedTargetsFile(fname_abs)) {
    const char* modal = nullptr;
    std::ostringstream e;
    MessageType messageType = MessageType::AUTHOR_WARNING;

    switch (this->Makefile->GetPolicyStatus(cmPolicies::CMP0024)) {
      case cmPolicies::WARN:
        e << cmPolicies::GetPolicyWarning(cmPolicies::CMP0024) << "\n";
        modal = "should";
      case cmPolicies::OLD:
        break;
      case cmPolicies::REQUIRED_IF_USED:
      case cmPolicies::REQUIRED_ALWAYS:
      case cmPolicies::NEW:
        modal = "may";
        messageType = MessageType::FATAL_ERROR;
    }
    if (modal) {
      e << "The file\n  " << fname_abs
        << "\nwas generated by the export() "
           "command.  It "
        << modal
        << " not be used as the argument to the "
           "include() command.  Use ALIAS targets instead to refer to targets "
           "by alternative names.\n";
      this->Makefile->IssueMessage(messageType, e.str());
      if (messageType == MessageType::FATAL_ERROR) {
        return false;
      }
    }
    gg->CreateGenerationObjects();
    gg->GenerateImportFile(fname_abs);
  }

  std::string listFile = cmSystemTools::CollapseFullPath(
    fname, this->Makefile->GetCurrentSourceDirectory());
  if (optional && !cmSystemTools::FileExists(listFile)) {
    if (!resultVarName.empty()) {
      this->Makefile->AddDefinition(resultVarName, "NOTFOUND");
    }
    return true;
  }

  bool readit =
    this->Makefile->ReadDependentFile(listFile.c_str(), noPolicyScope);

  // add the location of the included file if a result variable was given
  if (!resultVarName.empty()) {
    this->Makefile->AddDefinition(resultVarName,
                                  readit ? fname_abs.c_str() : "NOTFOUND");
  }

  if (!optional && !readit && !cmSystemTools::GetFatalErrorOccured()) {
    std::string m = "could not find load file:\n"
                    "  ";
    m += fname;
    this->SetError(m);
    return false;
  }
  return true;
}
