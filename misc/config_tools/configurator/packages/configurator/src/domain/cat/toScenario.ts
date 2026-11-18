import type {CacheAllocation, CacheRegion, CatView, ClosMaskEdit, Policy} from './model'

export function buildCacheRegion(view: CatView | null): CacheRegion | null {
    if (view === null || view.regions.length === 0) {
        return null
    }

    return {
        CACHE_ALLOCATION: view.regions.map(region => ({
            CACHE_ID: region.id,
            CACHE_LEVEL: region.level,
            POLICY: region.data.RTCore.concat(region.data.Standard, region.data.VCAT)
        }))
    }
}

/** Return a new CACHE_REGION with one policy mask changed or appended. */
export function setClosMask(cacheRegion: CacheRegion | null | undefined, edit: ClosMaskEdit): CacheRegion {
    const allocations: CacheAllocation[] = cacheRegion && Array.isArray(cacheRegion.CACHE_ALLOCATION)
        ? cacheRegion.CACHE_ALLOCATION.map(allocation => ({
            ...allocation,
            POLICY: Array.isArray(allocation.POLICY)
                ? allocation.POLICY.map(policy => ({...policy}))
                : []
        }))
        : []

    let allocation = allocations.find(candidate =>
        candidate.CACHE_ID === edit.cacheId && candidate.CACHE_LEVEL === edit.cacheLevel)
    if (!allocation) {
        allocation = {
            CACHE_ID: edit.cacheId,
            CACHE_LEVEL: edit.cacheLevel,
            POLICY: []
        }
        allocations.push(allocation)
    }

    const policy = allocation.POLICY.find(candidate =>
        candidate.VM === edit.vm && candidate.VCPU === edit.vcpu && candidate.TYPE === edit.type)
    if (policy) {
        policy.CLOS_MASK = edit.closMask
    } else {
        const newPolicy: Policy = {
            VM: edit.vm,
            VCPU: edit.vcpu,
            TYPE: edit.type,
            CLOS_MASK: edit.closMask
        }
        allocation.POLICY.push(newPolicy)
    }

    return {
        ...cacheRegion,
        CACHE_ALLOCATION: allocations
    }
}
