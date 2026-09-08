import { readFile } from "node:fs/promises"
import { join } from "node:path"

const OPEN_TODO_STATES = new Set(["pending", "in_progress"])
const MAX_UNCHANGED_RESUMES = 2

const inFlight = new Set()
const resumeState = new Map()

function openTodos(todos) {
  return todos.filter((todo) => OPEN_TODO_STATES.has(todo.status))
}

async function activeGoalPrefix(directory) {
  try {
    const ledger = await readFile(join(directory, "docs", "EXECUTION_LEDGER.md"), "utf8")
    const match = ledger.match(/^\*\*Active milestone:\*\* Goal (\d{1,2})\b/m)
    return match ? `[G${match[1].padStart(2, "0")}-` : null
  } catch {
    return null
  }
}

function todoSignature(todos) {
  return JSON.stringify(
    openTodos(todos)
      .map((todo) => ({
        id: todo.id ?? "",
        content: todo.content ?? "",
        status: todo.status,
        priority: todo.priority ?? "",
      }))
      .sort((left, right) =>
        `${left.id}\u0000${left.content}`.localeCompare(`${right.id}\u0000${right.content}`),
      ),
  )
}

function continuationPrompt(attempt) {
  return `Automatic continuation ${attempt} of ${MAX_UNCHANGED_RESUMES} for this unchanged todo state.

Before doing more work, read AGENTS.md and docs/EXECUTION_LEDGER.md. Reconcile the session-local todos against the durable ledger. Session titles, old summaries, and provisional later-goal files are not milestone authority.

If an open todo is already verified or superseded, close or cancel it now. Otherwise select one open todo whose content begins with the active milestone's stable acceptance ID, perform the next safe action, verify it, and update the todo and durable ledger as soon as evidence changes. Do not rerun an unchanged command or reopen a verified criterion without new failing evidence.

After two unsuccessful attempts on the same criterion, use the required review or a materially different diagnostic. If no safe progress remains, record the exact blocker and close every open todo rather than leaving another stale continuation trigger.`
}

export const ContinuousExecution = async ({ client, directory }) => ({
  event: async ({ event }) => {
    if (event.type === "session.deleted") {
      const deletedSessionID = event.properties?.info?.id ?? event.properties?.sessionID
      if (deletedSessionID) resumeState.delete(deletedSessionID)
      return
    }

    if (event.type !== "session.idle") return

    const sessionID = event.properties?.sessionID
    if (!sessionID || inFlight.has(sessionID)) return

    inFlight.add(sessionID)
    try {
      const response = await client.session.todo({
        path: { id: sessionID },
        query: { directory },
      })
      const todos = response.data ?? []
      const prefix = await activeGoalPrefix(directory)
      const open = prefix
        ? openTodos(todos).filter((todo) => todo.content?.startsWith(prefix))
        : []

      if (open.length === 0) {
        resumeState.delete(sessionID)
        return
      }

      const signature = todoSignature(open)
      const previous = resumeState.get(sessionID)
      const unchangedResumes = previous?.signature === signature ? previous.count : 0

      if (unchangedResumes >= MAX_UNCHANGED_RESUMES) return

      const attempt = unchangedResumes + 1
      resumeState.set(sessionID, { signature, count: attempt })

      await client.session.promptAsync({
        path: { id: sessionID },
        query: { directory },
        body: {
          parts: [
            {
              type: "text",
              text: continuationPrompt(attempt),
              synthetic: true,
            },
          ],
        },
      })
    } finally {
      inFlight.delete(sessionID)
    }
  },
})
