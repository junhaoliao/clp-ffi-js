import ModuleInit from "./cmake-build-debug/ClpIrV1Decoder.js"
import fs from "node:fs"

const main = async () => {
    console.time()
    const file = fs.readFileSync("./test.clp.zst")
    // const file = fs.readFileSync("./test.txt")
    const Module = await ModuleInit()

    const decoder = new Module.ClpIrV1Decoder(new Uint8Array(file))
    let estimatedNumEvents = decoder.estimatedNumEvents()
    console.log(decoder.buildIdx(0, estimatedNumEvents))
    estimatedNumEvents = decoder.estimatedNumEvents()

    const results = decoder.decode(0, estimatedNumEvents)
    console.timeEnd()
    console.log(results.slice(0, 10))
}

void main()