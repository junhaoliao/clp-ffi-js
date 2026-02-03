/**
 * Abstract base class for CLP archive search queries.
 */
abstract class ClpQuery {
    abstract readonly queryLanguage: string;

    abstract getQueryString(): string;
}

/**
 * Kibana Query Language (KQL) query.
 *
 * @example
 * const query = new KqlQuery('level: ERROR AND attr.ctx: "*conn1*"');
 */
class KqlQuery extends ClpQuery {
    readonly queryLanguage = "kql";

    readonly #queryString: string;

    constructor(query: string) {
        super();
        this.#queryString = query;
    }

    getQueryString(): string {
        return this.#queryString;
    }
}

export {
    ClpQuery,
    KqlQuery,
};
