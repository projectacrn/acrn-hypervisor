import Welcome from "./pages/Welcome.vue";
import Config from "./pages/Config.vue";

import {createRouter, createWebHashHistory} from 'vue-router'

const routes = [
    {name: "Welcome", path: '/', component: Welcome},
    {name: "Config", path: '/config/:WorkingFolder', component: Config, props: route => ({
        WorkingFolder: route.params.WorkingFolder,
        isNewConfig: route.query.isNewConfig
    })},
]

const router = createRouter({
    history: createWebHashHistory(),
    routes,
})
export default router;
