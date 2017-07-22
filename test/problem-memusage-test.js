'use strict;'

const expect = require('code').expect
const testRoot = require('path').resolve(__dirname, '..')
const glp = require('bindings')({ module_root: testRoot, bindings: 'glpk' })
const temp = require('temp').track()
const fs = require('fs')
const setupSimplexLP = require('./setup_simplex.js').setupSimplexLP

glp.termOutput(false)

describe('Verify problem.memStats', function() {
    it('should have meminfo as the problem is processed asynchronously, and when done', function(done) {
        let lp = setupSimplexLP()
        let idx = 0;
        let maxObserved = 0
        lp.on('log', function(msg) {
            let info = lp.memStats()

            expect(info).to.be.an.object()
            expect(info).to.include(['count','cpeak','total','tpeak'])
            expect(info.count).to.be.a.number()
            expect(info.cpeak).to.be.a.number()
            expect(info.total).to.be.a.number()
            expect(info.tpeak).to.be.a.number()
            expect(info.tpeak).to.be.at.least(info.total)
            expect(info.cpeak).to.be.at.least(info.count)
            if (maxObserved < info.total) {
                maxObserved = info.total
            }
        })
        lp.intopt({ msgLev: glp.MSG_ALL, presolve: glp.ON }, function() {
            let info = lp.memStats()
            let globalInfo = glp.glpMemInfo()

            expect(info).to.be.an.object()
            expect(info).to.include(['count','cpeak','total','tpeak'])
            expect(info.count).to.be.a.number()
            expect(info.cpeak).to.be.a.number()
            expect(info.total).to.be.a.number()
            expect(info.tpeak).to.be.a.number()
            expect(info.tpeak).to.be.at.least(info.total)
            expect(info.cpeak).to.be.at.least(info.count)
            expect(info.tpeak).to.be.at.least(maxObserved)
            expect(info.tpeak).to.be.at.least(20000)
            lp.delete()

            done();
        })
    })
})

