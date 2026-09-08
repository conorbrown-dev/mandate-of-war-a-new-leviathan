import assert from "node:assert/strict"

import { ContinuousExecution } from "../.opencode/plugins/continuous-execution.js"

let todos = []
const prompts = []
const client = {
  session: {
    todo: async () => ({ data: todos }),
    promptAsync: async (request) => {
      prompts.push(request)
    },
  },
}

const hooks = await ContinuousExecution({ client, directory: process.cwd() })
const idle = (sessionID) =>
  hooks.event({ event: { type: "session.idle", properties: { sessionID } } })

todos = [{ id: "1", content: "[G03-COMBAT-BENCH] Build benchmark", status: "pending", priority: "high" }]
await idle("bounded")
await idle("bounded")
await idle("bounded")
assert.equal(prompts.length, 2, "an unchanged todo state must resume at most twice")
assert.equal(prompts[0].body.parts[0].synthetic, true)
assert.match(prompts[1].body.parts[0].text, /2 of 2/)

todos = [{ id: "1", content: "[G03-COMBAT-BENCH] Verify benchmark", status: "in_progress", priority: "high" }]
await idle("bounded")
assert.equal(prompts.length, 3, "changed todo content must reset the resume budget")

todos = [{ id: "1", content: "[G03-COMBAT-BENCH] Verify benchmark", status: "completed", priority: "high" }]
await idle("bounded")
assert.equal(prompts.length, 3, "completed todos must not resume")

todos = [{ id: "2", content: "[G03-DATA] Add schema validation", status: "pending", priority: "high" }]
await hooks.event({ event: { type: "file.edited", properties: { file: "ignored" } } })
assert.equal(prompts.length, 3, "unrelated events must not resume")

todos = [{ id: "3", content: "Update documentation", status: "pending", priority: "high" }]
await idle("stale-unscoped")
assert.equal(prompts.length, 3, "an unscoped stale todo must not resume")

todos = [{ id: "4", content: "[G04-LOGISTICS] Repeat later-goal work", status: "pending", priority: "high" }]
await idle("wrong-goal")
assert.equal(prompts.length, 3, "a todo from a gated goal must not resume")

console.log("continuous execution guard: 6 behaviors passed")
