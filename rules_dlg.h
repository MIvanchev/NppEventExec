#ifndef __RULES_H__
#define __RULES_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opens a modal dialog which allows the user to create, modify, delete, reorder
 * etc. rules.
 * \param activeRules a pointer to a variable holding the rules currently used
 *        by the plugin; the list will be overwritten by the dialog after the
 *        changes are confirmed and all currently scheduled rules are executed
 *        or or aborted.
 * \param rule the rule that the user will be editing.
 * \return 1 when the dialog is closed.
 * \return 0 upon an error.
 * \return -1 upon an error.
 */
int openRulesDlg(Rule **activeRules);

#ifdef __cplusplus
}
#endif

#endif /* __RULES_H__ */