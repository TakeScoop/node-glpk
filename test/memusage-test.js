'use strict;'

const iterate = require('leakage').iterate
const expect = require('code').expect
const testRoot = require('path').resolve(__dirname, '..')
const glp = require('bindings')({ module_root: testRoot, bindings: 'glpk' })
const temp = require('temp').track()
const fs = require('fs')
const Promise = global.Promise
const setupSimplexLP = require('./setup_simplex').setupSimplexLP

glp.termOutput(false)

describe('Verify meminfo', function() {
    it('should have meminfo when handling log events fired as the problem is processed asynchronously', function(done) {
        let lp = new glp.Problem()
        let idx = 0;
        lp.on('log', function(msg) {
            let info = glp.glpMemInfo()
            expect(info).to.be.an.object()
            expect(info).to.include(['count','cpeak','total','tpeak'])
            expect(info.count).to.be.a.number()
            expect(info.cpeak).to.be.a.number()
            expect(info.total).to.be.a.number()
            expect(info.tpeak).to.be.a.number()
            expect(info.tpeak).to.be.at.least(info.total)
            expect(info.cpeak).to.be.at.least(info.count)
            if(idx++ >= 5) {
                done()
            }
        })
        lp.intopt({ msgLev: glp.MSG_ALL, presolve: glp.ON }, function() { })
    })
})


describe('Leakage check', function() {
    it('Should not leak memory', function() {
        this.timeout(15000)
        return iterate.async(() => {
            return new Promise((resolve, reject) => {
                let lp = setupSimplexLP()
                let idx = 0;
                lp.on('log', function(msg) {
                    let info = glp.glpMemInfo()
                    console.log(`INDEX: ${idx++}: ${msg.trim()}`)
                    expect(info).to.be.an.object()
                    expect(info).to.include(['count','cpeak','total','tpeak'])
                    expect(info.count).to.be.a.number()
                    expect(info.cpeak).to.be.a.number()
                    expect(info.total).to.be.a.number()
                    expect(info.tpeak).to.be.a.number()
                    expect(info.tpeak).to.be.at.least(info.total)
                    expect(info.cpeak).to.be.at.least(info.count)
                })

                lp.intopt({ msgLev: glp.MSG_ALL, presolve: glp.ON }, function() {
                    lp.delete()
                    resolve()
                })
            })
        })
    })
})

